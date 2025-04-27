#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>
#include <unordered_map>
#include <iomanip>
#include "utils.h"

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void listenThreadFunc(std::atomic<bool>& chatting, int serverfd) {
    while (chatting.load()) {
        // Listen for new Messages from server
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverfd, &readSet);
        
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms timeout
        
        int ready = select(serverfd + 1, &readSet, NULL, NULL, &timeout);

        if (ready > 0 && FD_ISSET(serverfd, &readSet)) {
            Message newMessage;
            receiveMessage(serverfd, newMessage);
            std::cout << "\033[2K\r";
            std::cout << "[" << formatTimestamp(std::stoi(timePointToString(newMessage.timestamp))) << "] " << newMessage.sender << ": " << newMessage.content << std::endl;
            std::cout.flush();
            std::cout << "Send a message > ";
            std::cout.flush();
        }
    }
}

class ChatClient {
private:
    sockaddr_in serveraddr;
    int serverfd;
    std::string username;
    std::string token;
    std::atomic<bool> chatting;
    const std::unordered_map<std::string, std::string> commands = {
        {"h", "Show this help"},
        {"q", "Disconnect and quit"},
        {"whoami", "Show your username"},
        {"users", "List online users"},
        {"chat", "Enter the chatroom"},
    };
public:

    ChatClient(char* serverIP, int port) {
        sockaddr_in serveraddr;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(port);
        serveraddr.sin_addr.s_addr = inet_addr(serverIP);
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
            std::cerr << "Error connecting to server" << std::endl;
            exit(1);
        }
        chatting.store(false);
    }

    void authenticate() {
        std::cout << "Welcome to chat client!" << std::endl;
        std::cout << "Username: ";
        std::string user;
        std::cin >> user;
        std::cout << "Password: ";
        std::string password;
        std::cin >> password;
        Message message {
            .type = Message::Type::AUTH,
            .sender = user,
            .receiver = password,
            .content = "",
            .token = "",
            .timestamp = std::chrono::system_clock::now()
        };
        sendMessage(serverfd, message);
        receiveMessage(serverfd, message);
        token = message.token;
        if (token == "") {
            std::cout << "Authentication failed!" << std::endl;
            exit(1);
        }
        username = message.sender;
    }
    void printHelp() {
        std::cout << "Commands: " << std::endl;
        for (const auto& command : commands) {
            std::cout << "  " // small indent
                      << std::left << std::setw(15) << command.first // left-align, 15-character width
                      << command.second
                      << std::endl;
        }
    }

    std::vector<std::string> printChatroom() {
        std::cout << "Welcome to the Chat Room" << std::endl;
        std::cout << "Type the number of the user you want to chat with" << std::endl;
        Message message {
            .type = Message::Type::COMMAND,
            .sender = "",
            .receiver = "",
            .content = "allUsers",
            .token = token,
            .timestamp = std::chrono::system_clock::now()
        };
        sendMessage(serverfd, message);
        receiveMessage(serverfd, message);
        std::vector<std::string> users = split(message.content, '\n');
        for (int i = 0; i < users.size(); i++) {
            std::cout << "(" << i + 1 << ") " << users[i] << std::endl;
        }
        std::cout << "(q) back to menu" << std::endl;
        return users;
    }

    void clearScreen() {
        std::cout << "\033[2J\033[1;1H" << std::flush;
    }

    void chatroom() {
        clearScreen();
        std::vector<std::string> users = printChatroom();
        while (true) {
            std::cout << "> ";
            std::string chatInput;
            std::getline(std::cin, chatInput);
            if (chatInput == "q") {
                clearScreen();
                printHelp();
                return;
            }
            try {
                int userID = std::stoi(chatInput);
                if (userID < 1 || userID > users.size()) {
                    std::cout << "Invalid user ID" << std::endl;
                    continue;
                }
                chat(users[userID - 1]);
            }
            catch (const std::exception &e) {
                std::cerr << "Error: " << e.what() << std::endl;
                continue;
            }
        }
    }

    void chat(std::string otherUser) {
        clearScreen();
        std::cout << "Chat with " << otherUser << std::endl;
        std::cout << "Type \"!q\" to go back to menu" << std::endl;
        // Get chat history from server
        Message message {
            .type = Message::Type::COMMAND,
            .sender = username,
            .receiver = otherUser,
            .content = "chat",
            .token = token,
            .timestamp = std::chrono::system_clock::now()
        };
        sendMessage(serverfd, message);
        receiveMessage(serverfd, message);
        std::stringstream ss(message.content);
        std::string line;
        int lastPing = 0;
        while (std::getline(ss, line)) {
            std::stringstream msg(line);
            std::string timestamp, sender, content;
            msg >> timestamp >> sender;
            std::getline(msg, content);
            lastPing = std::stoi(timestamp);
            std::cout << "[" << formatTimestamp(lastPing) << "] " << sender << ":" << content << std::endl;
        }

        chatting.store(true);
        // Start a thread that pings the server for new chat messages every second
        std::thread listenThread(listenThreadFunc, std::ref(chatting), serverfd);

        while (true) {
            std::cout << "Send a message > ";
            std::string chatInput;
            std::getline(std::cin, chatInput);
            if (chatInput == "!q") {
                chatting.store(false);
                listenThread.join();
                clearScreen();
                printChatroom();
                return;
            }
            // Remove leading whitespace
            chatInput.erase(chatInput.begin(), std::find_if(chatInput.begin(), chatInput.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));

            std::cout << "\033[1A\033[2K\r";
            if (chatInput.empty()) {
                continue;
            }
            auto now = std::chrono::system_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
            std::cout << "[" << formatTimestamp(duration.count()) << "] " << username << ": " << chatInput << std::endl;
            // Send message to server about this chat
            message = {
                .type = Message::Type::CHAT,
                .sender = username,
                .receiver = otherUser,
                .content = chatInput,
                .token = token,
                .timestamp = now
            };
            sendMessage(serverfd, message);
        }
    }
    void run() {
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        clearScreen();
        std::cout << "Welcome " << username << "!" << std::endl;
        printHelp();
        while (true) {
            std::cout << "> ";
            std::string input;
            std::getline(std::cin, input);
            if (commands.find(input) == commands.end()) {
                std::cout << "Unknown command. Type \"h\" for help" << std::endl;
                continue;
            }
            if (input == "h") {
                printHelp();
            }
            else if (input == "q") {
                std::cout << "Quitting chat client. Thanks for chatting!" << std::endl;
                close(serverfd);
                exit(0);
            }
            else if (input == "whoami") {
                std::cout << username << std::endl;
            }
            else if (input == "users") {
                Message message {
                    .type = Message::Type::COMMAND,
                    .sender = "",
                    .receiver = "",
                    .content = "onlineUsers",
                    .token = token,
                    .timestamp = std::chrono::system_clock::now()
                };
                sendMessage(serverfd, message);
                receiveMessage(serverfd, message);
                std::cout << "Online users:" << std::endl;
                std::cout << message.content;
            }
            else if (input == "chat") {
                chatroom();
            }
        }
    }
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <serverIP> <port>" << std::endl;
        return 1;
    }
    // User login
    // std::cout << "Login successful!" << std::endl;
    try {
        int port = std::stoi(argv[2]);
        if (port < 0 || port > 65535) {
            std::cerr << "Invalid port number" << std::endl;
            return 1;
        }
        ChatClient client(argv[1], port);
        client.authenticate();
        client.run();
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}