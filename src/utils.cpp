#include "utils.h"
#include <random>

std::int64_t to_milliseconds_since_epoch(std::chrono::time_point<std::chrono::system_clock> timestamp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
}

std::chrono::time_point<std::chrono::system_clock> from_milliseconds_since_epoch(std::int64_t ms) {
    return std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(ms));
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

    // Serialize timestamp
    std::int64_t timestamp_ms = to_milliseconds_since_epoch(message.timestamp);
    uint64_t network_timestamp = htonll(timestamp_ms);

    ssize_t sent_bytes = write(sockfd, &network_timestamp, sizeof(network_timestamp));
    if (sent_bytes == -1) {
        std::cerr << "Failed to send data" << std::endl;
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
    uint64_t network_timestamp;
    if (read(sockfd, &network_timestamp, sizeof(network_timestamp)) < 0) {
        std::cerr << "Error receiving timestamp" << std::endl;
        return false;
    }

    // Convert timestamp back to host byte order and into a time_point
    std::int64_t timestamp_ms = ntohll(network_timestamp);
    message.timestamp = from_milliseconds_since_epoch(timestamp_ms);

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
