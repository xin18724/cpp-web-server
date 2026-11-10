cmake_minimum_required(VERSION 3.10)
project(cpp-web-server CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(server
    src/main.cpp
    src/server.cpp
)

target_link_libraries(server pthread)