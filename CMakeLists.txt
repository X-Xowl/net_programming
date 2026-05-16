cmake_minimum_required(VERSION 4.2)
project(untitled10)

set(CMAKE_CXX_STANDARD 23)

add_executable(untitled10 main.cpp
        main.cpp
        ThreadPool.h
        Epoll.cpp
        Epoll.h
        Socket.h)
