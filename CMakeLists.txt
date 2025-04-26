cmake_minimum_required(VERSION 3.10)

# Project name
project(Chat)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(SQLite3 REQUIRED)

# Build client
add_executable(client src/client.cpp src/utils.h src/utils.cpp)
target_include_directories(client PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(client PRIVATE ${SQLite3_LIBRARIES})

# Build server
add_executable(server src/server.cpp src/utils.h src/utils.cpp)
target_include_directories(server PRIVATE ${SQLite3_INCLUDE_DIRS})
target_link_libraries(server PRIVATE ${SQLite3_LIBRARIES})