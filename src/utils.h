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
        AUTH,
        COMMAND,
        CLOSE,
    };
    Type type;
    std::string sender; // Field for username on auth
    std::string receiver; // Field for password on auth
    std::string content; // Field for command on command, content is returned in this field too
    std::string token;
    std::chrono::time_point<std::chrono::system_clock> timestamp;
};

std::string timePointToString(std::chrono::system_clock::time_point time);

bool sendMessage(int sockfd, const Message& message);

bool receiveMessage(int sockfd, Message& message);

std::string generateRandomToken();

std::string formatTimestamp(int64_t timestampSeconds);

std::chrono::system_clock::time_point intToTimePoint(int timestamp);

void emptySocket(int sockfd);