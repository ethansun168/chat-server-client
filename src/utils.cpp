#include "utils.h"
#include <fcntl.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>


std::string timePointToString(std::chrono::system_clock::time_point time) {
    auto unix_timestamp = std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
    return std::to_string(unix_timestamp);
}

uint64_t htonll(uint64_t value) {
    return ((static_cast<uint64_t>(htonl(static_cast<uint32_t>(value))) << 32) |
            htonl(static_cast<uint32_t>(value >> 32)));
}

// Convert 64-bit value from network byte order to host byte order
uint64_t ntohll(uint64_t value) {
    return ((static_cast<uint64_t>(ntohl(static_cast<uint32_t>(value))) << 32) |
            ntohl(static_cast<uint32_t>(value >> 32)));
}

bool sendString(int sockfd, const std::string& message) {
    size_t messageSize = message.size();
    if (write(sockfd, &messageSize, sizeof(messageSize)) < 0) {
        return false;
    }
    if (write(sockfd, message.c_str(), messageSize) < 0) {
        return false;
    }
    return true;
}

bool receiveString(int sockfd, std::string& message) {
    size_t messageSize;
    if (read(sockfd, &messageSize, sizeof(messageSize)) <= 0) {
        return false;
    }
    message.resize(messageSize);
    if (read(sockfd, &message[0], messageSize) < 0) {
        return false;
    }
    return true;
}

bool sendMessage(int sockfd, const Message& message) {
    Message::Type type = message.type;
    int32_t typeInt = static_cast<int32_t>(type);

    if (write(sockfd, &typeInt, sizeof(typeInt)) < 0) {
        return false;
    }

    if (!sendString(sockfd, message.sender)) {
        return false;
    }

    if (!sendString(sockfd, message.receiver)) {
        return false;
    }

    if (!sendString(sockfd, message.content)) {
        return false;
    }

    if (!sendString(sockfd, message.token)) {
        return false;
    }

    std::string timestamp = timePointToString(message.timestamp);
    if(!sendString(sockfd, timestamp)) {
        return false;
    }

    return true;
}

bool receiveMessage(int sockfd, Message& message) {
    int32_t typeInt;
    if (read(sockfd, &typeInt, sizeof(typeInt)) < 0) {
        return false;
    }
    message.type = static_cast<Message::Type>(typeInt);
    
    if (!receiveString(sockfd, message.sender)) {
        return false;
    }

    if (!receiveString(sockfd, message.receiver)) {
        return false;
    }

    if (!receiveString(sockfd, message.content)) {
        return false;
    }

    if (!receiveString(sockfd, message.token)) {
        return false;
    }

    // Receive timestamp
    std::string timestamp;
    if (!receiveString(sockfd, timestamp)) {
        return false;
    }
    message.timestamp = intToTimePoint(std::stoi(timestamp));

    return true;
}

std::string generateRandomToken() {
    static const std::string characters =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, characters.size() - 1);

    std::string token;
    for (size_t i = 0; i < 32; ++i) {
        token += characters[dist(rng)];
    }

    return token;
}

std::string formatTimestamp(int64_t timestampSeconds) {
    // Convert seconds to time_t
    std::time_t tt = static_cast<std::time_t>(timestampSeconds);
    
    // Convert time_t to tm structure
    std::tm tm = *std::localtime(&tt);

    // Format using ostringstream
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");  // Format as "YYYY-MM-DD HH:MM:SS"
    return oss.str();
}

std::chrono::system_clock::time_point intToTimePoint(int timestamp) {
    return std::chrono::system_clock::time_point{std::chrono::seconds(timestamp)};
}

void emptySocket(int sockfd) {
    // Set the socket to non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

    // Drain the socket
    char buffer[1024];
    while (true) {
        ssize_t bytes = recv(sockfd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            break; // No more data or error
        }
    }

    // Restore blocking mode
    fcntl(sockfd, F_SETFL, flags);
}