//
// Created by MY on 2026/5/16.
//

#ifndef UNTITLED10_EPOLL_H
#define UNTITLED10_EPOLL_H

#include <stdexcept>
#include <unistd.h>
#include <sys/epoll.h>
#include <array>
#include <string>
#include <span>
#include <memory>
#include <unordered_map>
#include "Socket.h"
struct Client
{
    explicit Client(std::unique_ptr<Socket>&& socket):socket(std::move(socket)){}
    std::unique_ptr<Socket> socket;
    std::string out_buffer{};
};
class Epoll
{
public:
    Epoll();
    ~Epoll();
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
    Epoll(Epoll&& other) noexcept;
    Epoll& operator=(Epoll&& other) noexcept;
    void add(int socket_fd,uint32_t events) const;
    void modify(int socket_fd,uint32_t events) const;
    void del(int socket_fd) const;
    int wait(std::span<epoll_event> event, int timeout) const;
    void close_client(int client_fd);
    void accept_clients(Socket& listen_socket);
    void handle_read(int client_fd);
    void handle_write(int client_fd);
private:
    int epoll_fd;
    std::unordered_map<int,Client> clients;
};

#endif //UNTITLED10_EPOLL_H
