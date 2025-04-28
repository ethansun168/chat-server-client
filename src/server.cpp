#include <unordered_map>
#include <set>
#include "utils.h"

const std::string DATABASE = "../var/database.sqlite3";

class Database {
public:
    Database() {
        if (sqlite3_open(DATABASE.c_str(), &db) != SQLITE_OK) {
            std::cerr << "Error opening database" << std::endl;
            exit(1);
        }
    }

    ~Database() {
        if (db) {
            sqlite3_close(db);
        }
    }

    sqlite3* get() {
        return db;
    }
private:
    sqlite3* db;
};

class Statement {
public:
    Statement(sqlite3* db, const std::string& sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
            std::cerr << "Error creating statement" << std::endl;
            exit(1);
        }
    }

    ~Statement() {
        if (stmt) {
            sqlite3_finalize(stmt);
        }
    }

    void bindText(int index, const std::string& value) {
        if (sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            std::cerr << "Error binding text" << std::endl;
            exit(1);
        }
    }

    void bindInt(int index, int value) {
        if (sqlite3_bind_int(stmt, index, value) != SQLITE_OK) {
            std::cerr << "Error binding integer" << std::endl;
            exit(1);
        }
    }

    void clearBindings() {
        if (sqlite3_clear_bindings(stmt) != SQLITE_OK) {
            std::cerr << "Error clearing bindings" << std::endl;
            exit(1);
        }
    }

    void reset() {
        if (sqlite3_reset(stmt) != SQLITE_OK) {
            std::cerr << "Error resetting statement" << std::endl;
            exit(1);
        }
    }

    bool step() {
        return sqlite3_step(stmt) == SQLITE_ROW;
    }

    const char* getColumnText(int columnIndex) {
        return reinterpret_cast<const char*>(sqlite3_column_text(stmt, columnIndex));
    }

    sqlite3_stmt* get() {
        return stmt;
    }
private:
    sqlite3_stmt* stmt;
};

class ChatServer {
private:
    int serverfd;
    struct sockaddr_in serveraddr;
    // clientfd to token
    struct User {
        std::string username;
        std::string token;
    };
    std::unordered_map<int, User> clients;

public:
    ChatServer(int port) {
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        int optval = 1;
        setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        sockaddr_in serveraddr;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = INADDR_ANY;
        serveraddr.sin_port = htons(port);
        bind(serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
        std::cout << "Server started on port " << port << std::endl;
        listen(serverfd, 5);
    }

    ~ChatServer() {
        close(serverfd);
    }

    void run() {
        while (true) {
            // Use select to handle multiple clients
            fd_set readfds;
            FD_ZERO(&readfds);

            // Add server socket to set
            FD_SET(serverfd, &readfds);

            // Add client sockets to set
            for (auto& client : clients) {
                FD_SET(client.first, &readfds);
            }
            
            int activity = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
            if (activity < 0 && errno != EINTR) {
                std::cerr << "Error in select" << std::endl;
                continue;
            }

            if (FD_ISSET(serverfd, &readfds)) {
                // Handle new client connection
                sockaddr_in clientaddr;
                socklen_t clientaddr_len = sizeof(clientaddr);
                int clientfd = accept(serverfd, (struct sockaddr*)&clientaddr, &clientaddr_len);
                if (clientfd < 0) {
                    std::cerr << "Error in accept" << std::endl;
                    continue;
                }
                std::cout << "New client connected: " << inet_ntoa(clientaddr.sin_addr) << ":" << ntohs(clientaddr.sin_port) << std::endl;
                clients[clientfd] = {};
            }

            // Handle client activity
            for (auto it = clients.begin(); it != clients.end(); ) {
                int clientfd = it->first;
                // std::cout << clientfd << std::endl;
                if (FD_ISSET(clientfd, &readfds)) {
                    // Do something
                    Message message;
                    if (!receiveMessage(clientfd, message)) {
                        std::cerr << "Closing connection with " << clientfd << std::endl;
                        close(clientfd);
                        it = clients.erase(it++);
                        continue;
                    }
                    if (message.type == Message::Type::AUTH) {
                        std::cout << "Received auth message. Username: " << message.sender << " password: " << message.receiver << std::endl;
                        // Check credentials
                        Database db;
                        Statement stmt(db.get(), "SELECT * FROM users WHERE username = ? AND password = ?");
                        stmt.bindText(1, message.sender);
                        stmt.bindText(2, message.receiver);
                        int result = sqlite3_step(stmt.get());
                        if (result == SQLITE_ROW) {
                            std::cout << "Authentication successful" << std::endl;
                            std::string token = generateRandomToken();
                            message.token = token;
                            sendMessage(clientfd, message);
                            clients[clientfd] = {message.sender, token};
                        }
                        else {
                            std::cout << "Authentication failed" << std::endl;
                            sendMessage(clientfd, message);
                            close(clientfd);
                            it = clients.erase(it++);
                            continue;
                        }
                    }

                    // Authenticate the message
                    if (message.token != clients[clientfd].token) {
                        std::cout << "Authentication failed" << std::endl;
                        Message responseMessage {
                            .type = Message::Type::AUTH,
                            .sender = "",
                            .receiver = "",
                            .content = "",
                            .token = "",
                            .timestamp = std::chrono::system_clock::now()
                        };
                        sendMessage(clientfd, responseMessage);
                        close(clientfd);
                        it = clients.erase(it++);
                        continue;
                    }

                    if (message.type == Message::Type::CHAT) {
                        if (message.receiver == "") {
                            std::cout << "Global chat: " << message.sender << ": " << message.content << std::endl;
                            // Insert into global_messages table
                            Database db;
                            Statement stmt(db.get(), "SELECT id from users where username = ?");
                            stmt.bindText(1, message.sender);
                            int userID = -1;
                            if (stmt.step()) {
                                userID = sqlite3_column_int(stmt.get(), 0);
                            }

                            Statement stmt2(db.get(), "INSERT INTO global_messages (sender_id, message) VALUES (?, ?)");
                            stmt2.bindInt(1, userID);
                            stmt2.bindText(2, message.content);
                            int result = sqlite3_step(stmt2.get());
                            if (result != SQLITE_DONE) {
                                std::cerr << "Error inserting into global_messages table" << std::endl;
                            }
                        }
                        else {
                            std::cout << message.sender << " -> " << message.receiver << ": " << message.content << std::endl;
                            // Insert chat into db
                            Database db;
                            Statement stmt(db.get(), "SELECT id FROM users WHERE username = ?");
                            stmt.bindText(1, message.sender);
                            int user1ID = -1;
                            int user2ID = -1;
                            if (stmt.step()) {
                                user1ID = sqlite3_column_int(stmt.get(), 0);
                            }
                            stmt.reset();
                            stmt.clearBindings();
                            stmt.bindText(1, message.receiver);
                            if (stmt.step()) {
                                user2ID = sqlite3_column_int(stmt.get(), 0);
                            }
                            Statement stmt2(db.get(), "INSERT INTO messages (sender_id, receiver_id, message) VALUES (?, ?, ?)");
                            stmt2.bindInt(1, user1ID);
                            stmt2.bindInt(2, user2ID);
                            stmt2.bindText(3, message.content);
                            int result = sqlite3_step(stmt2.get());
                            if (result != SQLITE_DONE) {
                                std::cerr << "Error inserting message into db" << std::endl;
                            }
                        }
                        // Send message to receiver clients if they are online
                        for (auto& client : clients) {
                            if (client.second.username != message.sender) {
                                bool shouldSend = (message.receiver.empty()) || (client.second.username == message.receiver);
                                if (shouldSend) {
                                    std::cout << "Sending message to " << message.receiver << std::endl;
                                    Message responseMessage {
                                        .type = Message::Type::CHAT,
                                        .sender = message.sender,
                                        .receiver = message.receiver,
                                        .content = message.content,
                                        .token = client.second.token,
                                        .timestamp = message.timestamp
                                    };
                                    sendMessage(client.first, responseMessage);
                                }
                            }
                        }
                    }
                    else if (message.type == Message::Type::COMMAND) {
                        if (message.content == "onlineUsers") {
                            // List online users
                            std::string response = "";
                            std::set<std::string> onlineUsers;
                            for (auto& client : clients) {
                                if (client.second.token != "") {
                                    onlineUsers.insert(client.second.username);
                                }
                            }

                            if (onlineUsers.empty()) {
                                response = "No users online\n";
                            }
                            else {
                                for (auto& user : onlineUsers) {
                                    response += user + "\n";
                                }
                            }
                            Message responseMessage {
                                .type = Message::Type::COMMAND,
                                .sender = "",
                                .receiver = "",
                                .content = response,
                                .token = message.token,
                                .timestamp = std::chrono::system_clock::now()
                            };
                            std::cout << "Responding with: " << std::endl;
                            std::cout << response;
                            sendMessage(clientfd, responseMessage);
                        }
                        else if (message.content == "allUsers") {
                            Database db;
                            Statement stmt(db.get(), "SELECT username FROM users");
                            int result = sqlite3_step(stmt.get());
                            std::string response = "";
                            while (result == SQLITE_ROW) {
                                response += std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0))) + "\n";
                                result = sqlite3_step(stmt.get());
                            }
                            Message responseMessage {
                                .type = Message::Type::COMMAND,
                                .sender = "",
                                .receiver = "",
                                .content = response,
                                .token = message.token,
                                .timestamp = std::chrono::system_clock::now()
                            };
                            std::cout << "Responding with: " << std::endl;
                            std::cout << response;
                            sendMessage(clientfd, responseMessage);
                        }
                        else if (message.content == "chat") {
                            std::cout << "Retreiving chat history between " << message.sender << " and " << message.receiver << std::endl;
                            Database db;
                            Statement stmt(db.get(), "SELECT u1.username AS sender, u2.username AS receiver, m.message, m.timestamp "
                                 "FROM messages m "
                                 "JOIN users u1 ON m.sender_id = u1.id "
                                 "JOIN users u2 ON m.receiver_id = u2.id "
                                 "WHERE (u1.username = ? AND u2.username = ?) "
                                 "OR (u1.username = ? AND u2.username = ?) "
                                 "ORDER BY m.timestamp;");

                            // Bind the parameters
                            stmt.bindText(1, message.sender);
                            stmt.bindText(2, message.receiver);
                            stmt.bindText(3, message.receiver);
                            stmt.bindText(4, message.sender);
                            std::string response = "";
                            while (stmt.step()) {
                                const char* sender = stmt.getColumnText(0);
                                const char* receiver = stmt.getColumnText(1);
                                const char* message = stmt.getColumnText(2);
                                const char* timestamp = stmt.getColumnText(3);
                                response += std::string(timestamp) + " " + std::string(sender) + " " + std::string(message) + "\n";
                            }
                            std::cout << response << std::endl;
                            Message responseMessage {
                                .type = Message::Type::COMMAND,
                                .sender = "",
                                .receiver = "",
                                .content = response,
                                .token = message.token,
                                .timestamp = std::chrono::system_clock::now()
                            };
                            sendMessage(clientfd, responseMessage);
                        }
                        else if (message.content == "globalChat") {
                            // Retrieve global chat history
                            Database db;
                            
                            Statement stmt(db.get(), "SELECT u.username, m.message, m.timestamp "
                                 "FROM global_messages m "
                                 "JOIN users u ON m.sender_id = u.id "
                                 "ORDER BY m.timestamp;");

                            std::string response = "";
                            while (stmt.step()) {
                                const char* sender = stmt.getColumnText(0);
                                const char* message = stmt.getColumnText(1);
                                const char* timestamp = stmt.getColumnText(2);
                                response += std::string(timestamp) + " " + std::string(sender) + " " + std::string(message) + "\n";
                            }
                            Message responseMessage {
                                .type = Message::Type::COMMAND,
                                .sender = "",
                                .receiver = "",
                                .content = response,
                                .token = message.token,
                                .timestamp = std::chrono::system_clock::now()
                            };
                            sendMessage(clientfd, responseMessage);

 
                        }
                    }

                }
                it++;
            }

        }
    }

};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    try {
        int port = std::stoi(argv[1]);
        if (port < 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
            return 1;
        }
        ChatServer server(port);
        server.run();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}