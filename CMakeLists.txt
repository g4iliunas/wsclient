cmake_minimum_required(VERSION 3.10)
project(wsclient)
add_executable(wsclient main.c src/wsclient.c src/yyjson.c)
find_package(Libevent REQUIRED)
target_link_libraries(wsclient libevent::core libevent::openssl)