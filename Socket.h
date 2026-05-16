//
// Created by MY on 2026/5/16.
//

#ifndef UNTITLED10_SOCKET_H
#define UNTITLED10_SOCKET_H

#include <stdexcept>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <mutex>
//Socket类，封装了socket的创建、绑定、监听、接受连接等操作，简化了代码的编写。
class Socket
{
public:
    Socket(const int domain, const int type, const int protocol)
    {
        fd = socket(domain, type, protocol);
        if (fd == -1)
        {
            throw std::runtime_error("Failed to create socket");
        }
    }

    explicit Socket(const int fd) : fd(fd)
    {
        if (fd == -1)
        {
            throw std::runtime_error("Invalid socket file descriptor");
        }
    }
    // 禁止复制，确保Socket对象的唯一性。
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) noexcept
    {
        fd = other.fd;
        other.fd = -1;
        addr_ = other.addr_;
        ip = other.ip;
        addr_len = other.addr_len;
    }
    Socket& operator=(Socket&& other) noexcept
    {
        if (this != &other)
        {
            if (fd != -1)
            {
                close(fd);
            }
            fd = other.fd;
            other.fd = -1;
            addr_ = other.addr_;
            ip = other.ip;
            addr_len = other.addr_len;
        }
        return *this;
    }
     ~Socket()
    {
        if (fd != -1)
        {
            close(fd);
        }
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "socket closed: "
                 << ip << ":"
                 << ntohs(addr_.sin_port) << std::endl;
        }
    }
    [[nodiscard]] int get_fd() const
    {
        return fd;
    }
    void bind()
     {
         if (::bind(fd, reinterpret_cast<sockaddr*>(&addr_), sizeof(addr_)) == -1)
         {
             throw std::runtime_error("Failed to bind socket");
         }
     } //将socket绑定到指定的地址和端口上，如果绑定失败则抛出异常。
    void listen(const int backlog) const
    {
        if (::listen(fd, backlog) == -1)
        {
            throw std::runtime_error("Failed to listen on socket");
        }
    }//将socket设置为监听状态，等待客户端连接，如果监听失败则抛出异常。
    void setsockopt(const int level,const int optname, int& opt) const
     {
         if (::setsockopt(fd, level, optname, &opt, sizeof(opt)) == -1)
         {
             throw std::runtime_error("Failed to set socket options");
         }
     }//设置socket选项，例如SO_REUSEADDR，允许地址重用，如果设置失败则抛出异常。
    ssize_t recv(char* buffer, const int len, const int flags) const
    {
         ssize_t n = ::recv(fd, buffer, len, flags);
         return n;
    }//从socket接收数据，返回接收到的字节数，如果接收失败则返回-1。
    bool send_all(const char* data, const size_t len) const
    {
         size_t sent = 0;
         while (sent < len)
         {
             ssize_t n = ::send(fd, data + sent, len - sent, 0);
             if (n <= 0)
             {
                 return false;
             }
             sent += n;
         }
         return true;
    }//向socket发送数据，确保所有数据都被发送，如果发送失败则返回false。
    Socket accept()
    {
        sockaddr_in addr{};
        socklen_t addr_len = sizeof(addr);
        const int client_fd = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        if (client_fd == -1)
        {
            throw std::runtime_error("Failed to accept connection");
        }
        Socket client_socket(client_fd);
        client_socket.set_addr(addr);
        return client_socket;
    } //接受一个新的连接，并返回一个新的Socket对象，如果接受失败则抛出异常。
    void set_addr(const sockaddr_in& addr)
    {
        addr_ = addr;
        std::array<char,INET_ADDRSTRLEN> ip_{};
        inet_ntop(
            AF_INET,
            &addr_.sin_addr,
            ip_.data(),
            ip_.size()
        );
        ip = std::string(ip_.data());
    }
    sockaddr_in get_addr()
    {
        return addr_;
    }
    void handle_client()
    {
        auto client_port = ntohs(addr_.sin_port);
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "client connected: "
                 << ip << ":"
                 << client_port << '\n'
                 << "thread id: " << std::this_thread::get_id() << std::endl;
        }
        std::array<char,4096> buffer{};
        while (true)
        {
            ssize_t n = recv(buffer.data(),buffer.size(),0);
            if (n > 0)
            {
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << n << "bytes from " << ip << ":" << client_port << '\n' << std::string_view(buffer.data(),n) << std::endl;
                }
                send_all(buffer.data(), n);
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "echoed back to " << ip << ":" << client_port << std::endl;
                }
            }
            else if (n == 0)
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout  << "client disconnected: " << ip << ":" << client_port << std::endl;
                break;
            }
            else
            {
                if (errno == EINTR)
                {
                    continue;
                }
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "recv failed from "
                     << ip << ":" << client_port
                     << " with error: " << strerror(errno) << std::endl;
                break;
            }
        }
    }
private:
    int fd;
    sockaddr_in addr_;
    std::string ip;
    socklen_t addr_len = sizeof(addr_);
    std::mutex cout_mutex;
};

#endif //UNTITLED10_SOCKET_H
