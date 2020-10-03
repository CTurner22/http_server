cmake_minimum_required(VERSION 3.13.4)
project(web_server)

set(CMAKE_CXX_STANDARD 14)

add_executable(web_server http_server.cc)