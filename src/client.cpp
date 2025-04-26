#include "utils.h"

class ChatClient {
private:
    sockaddr_in serveraddr;
public:

    ChatClient(char* serverIP, int port) {
        sockaddr_in serveraddr;
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_port = htons(port);
        serveraddr.sin_addr.s_addr = inet_addr(serverIP);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
            std::cerr << "Error connecting to server" << std::endl;
            exit(1);
        }
        std::cout << "Welcome to chat client!" << std::endl;
        std::cout << "Username: ";
        std::string username;
        std::cin >> username;
        std::cout << "Password: ";
        std::string password;
        std::cin >> password;
        Message message {
            .type = Message::Type::AUTH,
            .sender = username,
            .receiver = password,
            .content = "",
            .token = "",
            .timestamp = std::chrono::system_clock::now()
        };
        sendMessage(sockfd, message);
        receiveMessage(sockfd, message);
        std::string token = message.token;
        if (token == "") {
            std::cout << "Authentication failed!" << std::endl;
            exit(1);
        }
        std::cout << "Authentication successful!" << std::endl;
        // sendMessage(sockfd, message);
        // message.content = "another message";
        // sendMessage(sockfd, message);

        // message.type = Message::Type::AUTH;
        // message.content = "token";
        // sendMessage(sockfd, message);
    }

    void run() {

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
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}