#include <vector>
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

class ChatClient {
private:
    sockaddr_in serveraddr;
    int serverfd;
    std::string username;
    std::string token;
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
        // username = "user1";
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

    void clearScreen() {
        std::cout << "\033[2J\033[1;1H" << std::flush;
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
                clearScreen();
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
                // Split message.content into vector 
                std::vector<std::string> users = split(message.content, '\n');
                for (int i = 0; i < users.size(); i++) {
                    std::cout << "(" << i + 1 << ") " << users[i] << std::endl;
                }
                std::cout << "(q) back to menu" << std::endl;
                while (true) {
                    std::cout << "> ";
                    std::string chatInput;
                    std::getline(std::cin, chatInput);
                    if (chatInput == "q") {
                        clearScreen();
                        printHelp();
                        break;
                    }
                    try {
                        int userID = std::stoi(chatInput);
                        if (userID < 1 || userID > users.size()) {
                            std::cout << "Invalid user ID" << std::endl;
                            continue;
                        }
                        // Message message {
                        //     .type = Message::Type::COMMAND,
                        //     .sender = "",
                        //     .receiver = "",
                        //     .content = "chat " + users[userID - 1],
                        //     .token = token,
                        //     .timestamp = std::chrono::system_clock::now()
                        // };
                        // sendMessage(serverfd, message);
                        // receiveMessage(serverfd, message);
                        clearScreen();
                        std::cout << "Chat with " << users[userID - 1] << std::endl;
                        std::cout << message.content << std::endl;
                        std::cout << "Type \"q\" to go back to menu" << std::endl;
                    }
                    catch (const std::exception &e) {
                        std::cerr << "Error: " << e.what() << std::endl;
                        continue;
                    }
                }
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