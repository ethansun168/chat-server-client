#pragma once
#include <sqlite3.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <chrono>
#include <iostream>

struct Message {
    enum class Type {
        CHAT,
        AUTH
    };
    Type type;
    std::string sender; // Field for username on auth
    std::string receiver; // Field for password on auth
    std::string content;
    std::string token;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
};

std::int64_t to_milliseconds_since_epoch(std::chrono::time_point<std::chrono::system_clock> timestamp);

std::chrono::time_point<std::chrono::system_clock> from_milliseconds_since_epoch(std::int64_t ms);

bool sendMessage(int sockfd, const Message& message);

bool receiveMessage(int sockfd, Message& message);

std::string generateRandomToken();