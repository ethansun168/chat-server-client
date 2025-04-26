#include <unordered_map>
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
    // TODO: replace second int
    std::unordered_map<int, int> clients;

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
                clients[clientfd] = 0;
            }

            // Handle client activity
            for (auto it = clients.begin(); it != clients.end(); ) {
                int clientfd = it->first;
                // std::cout << clientfd << std::endl;
                if (FD_ISSET(clientfd, &readfds)) {
                    // Do something
                    Message message;
                    if (!receiveMessage(clientfd, message)) {
                        std::cerr << "Closing connection" << std::endl;
                        close(clientfd);
                        it = clients.erase(it++);
                        continue;
                    }
                    if (message.type == Message::Type::AUTH) {
                        std::cout << "Received auth message. Username: " << message.sender << " password: " << message.receiver << " at " << to_milliseconds_since_epoch(message.timestamp) << std::endl;
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
                            clients[clientfd] = 1;
                        }
                        else {
                            std::cout << "Authentication failed" << std::endl;
                            sendMessage(clientfd, message);
                            close(clientfd);
                            it = clients.erase(it++);
                            continue;
                        }
                    }
                    else if (message.type == Message::Type::CHAT) {
                        std::cout << "Received chat message from " << message.sender << ": " << message.content << " at " << to_milliseconds_since_epoch(message.timestamp) << std::endl;
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