//
// Created by MY on 2026/5/16.
//

#include "Epoll.h"

#include <cstring>
#include <iostream>
#include <ostream>

Epoll::Epoll()
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        throw std::runtime_error("epoll_create1 失败");
    }
}
Epoll::~Epoll()
{
    if (epoll_fd != -1)
    {
        close(epoll_fd);
    }
}
Epoll::Epoll(Epoll&& other) noexcept
{
    epoll_fd = other.epoll_fd;
    other.epoll_fd = -1;
}
Epoll& Epoll::operator=(Epoll&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    if (epoll_fd != -1)
    {
        close(epoll_fd);
    }
    epoll_fd = other.epoll_fd;
    other.epoll_fd = -1;
    return *this;
}
void Epoll::add(int socket_fd,uint32_t events) const
{
    epoll_event event{};
    event.events = events;
    event.data.fd = socket_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD,socket_fd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl 失败");
    }
}
void Epoll::modify(int socket_fd,uint32_t events) const
{
    epoll_event event{};
    event.events = events;
    event.data.fd = socket_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_fd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl 失败");
    }
}
void Epoll::del(int socket_fd) const
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_fd, nullptr) == -1)
    {
        throw std::runtime_error("epoll_ctl 失败");
    }
}
int Epoll::wait(std::span<epoll_event> events, int timeout) const
{
    while (true)
    {
        const int n = epoll_wait(epoll_fd, events.data(), static_cast<int>(events.size()), timeout);
        if (n == -1)
        {
            if (errno == EINTR)
            {
                continue; // 被信号打断，重试
            }
            std::cerr << "epoll_wait 失败: " << strerror(errno) << std::endl;
            throw std::runtime_error("epoll_wait 失败");
        }
        return n;
    }
}

void Epoll::accept_clients(Socket &listen_socket)
{
    while (true)
    {
        sockaddr_in addr{};
        socklen_t addr_len = sizeof(addr);
        int client_fd = accept(listen_socket.get_fd(), reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break; // No more clients to accept
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                std::cerr << "accept 失败: " << strerror(errno) << std::endl;
                throw std::runtime_error("accept 失败");
            }
        }
        auto client_socket_ptr = std::make_unique<Socket>(client_fd);
        client_socket_ptr->set_addr(addr);
        if (client_socket_ptr->set_nonblock() == -1)
        {
            std::cerr << "无法设置为非阻塞模式: " << strerror(errno) << std::endl;
            throw std::runtime_error("无法设置为非阻塞模式");
        }
        add(client_socket_ptr->get_fd(), EPOLLIN | EPOLLRDHUP);
        int fd = client_socket_ptr->get_fd();
        std::string ip = client_socket_ptr->get_ip();
        uint16_t port = client_socket_ptr->get_port();
        clients.emplace(fd,std::move(client_socket_ptr));
            std::cout << "新客户端连接: " << ip << ":"
                      << port << '\n'
                      << "fd= " << fd << std::endl;
    }
}

void Epoll::close_client(const int client_fd)
{
    try
    {
        del(client_fd);
    }
    catch (...){}
    clients.erase(client_fd);
    std::cout << "客户端断开: fd= " << client_fd << std::endl;
}

void Epoll::handle_read(const int client_fd)
{
    if (!clients.contains(client_fd))
    {
        return;
    }
    auto& client = clients.at(client_fd);
    std::array<char,4096> buffer{};
    while (true)
    {
        ssize_t n = client.socket->recv(buffer,0);
        if (n > 0)
        {
            std::cout << "收到 " << n << " 字节来自 " << client.socket->get_ip() << ":"
                 << client.socket->get_port() << '\n'
                 << std::string_view(buffer.data(),n) << std::endl;
            client.out_buffer.append(buffer.data(),n);
        }
        else if (n == 0)
        {
            std::cout << "客户端断开: " << client.socket->get_ip() << ":"
                 << client.socket->get_port() << std::endl;
            close_client(client_fd);
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            std::cerr << "recv 失败来自 " << client.socket->get_ip() << ":"
                 << client.socket->get_port() << " 错误: " << strerror(errno) << std::endl;
            close_client(client_fd);
            break;
        }
    }
    if (!clients.contains(client_fd))
    {
        return;
    }
    auto& out_buffer = clients.at(client_fd).out_buffer;
    auto& client_socket_ptr = clients.at(client_fd).socket;
    if (!out_buffer.empty())
    {
        if (!client_socket_ptr->flush_output(out_buffer))
        {
            close_client(client_fd);
            return;
        }
        if (!out_buffer.empty())
        {
            modify(client_fd,EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        }
        else
        {
            modify(client_fd,EPOLLIN | EPOLLRDHUP);
        }
    }
}

void Epoll::handle_write(int client_fd)
{
    if (!clients.contains(client_fd))
    {
        return;
    }
    auto& client = clients.at(client_fd);
    auto& out_buffer = client.out_buffer;
    auto& client_socket_ptr = client.socket;
    if (!client_socket_ptr->flush_output(out_buffer))
    {
        close_client(client_fd);
        return;
    }
    if (out_buffer.empty())
    {
        modify(client_fd,EPOLLIN | EPOLLRDHUP);
    }
    else
    {
        modify(client_fd,EPOLLIN | EPOLLOUT | EPOLLRDHUP);
    }
}
