//
// Created by MY on 2026/5/16.
//

#ifndef UNTITLED10_SOCKET_H
#define UNTITLED10_SOCKET_H

#include <stdexcept>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <mutex>
#include <span>
#include <thread>
//Socket类，封装了socket的创建、绑定、监听、接受连接等操作，简化了代码的编写。
class Socket
{
public:
    Socket(const int domain, const int type, const int protocol)
    {
        fd = socket(domain, type, protocol);
        if (fd == -1)
        {
            throw std::runtime_error("创建socket失败");
        }
    }

    explicit Socket(const int fd) : fd(fd)
    {
        if (fd == -1)
        {
            throw std::runtime_error("非法socket文件描述符");
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
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "socket 关闭: "
                     << ip << ":"
                     << ntohs(addr_.sin_port) << std::endl;
            }
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
             throw std::runtime_error("socket 绑定失败");
         }
     } //将socket绑定到指定的地址和端口上，如果绑定失败则抛出异常。
    void listen(const int backlog) const
    {
        if (::listen(fd, backlog) == -1)
        {
            throw std::runtime_error("监听socket失败");
        }
    }//将socket设置为监听状态，等待客户端连接，如果监听失败则抛出异常。
    void setsockopt(const int level,const int optname, int& opt) const
     {
         if (::setsockopt(fd, level, optname, &opt, sizeof(opt)) == -1)
         {
             throw std::runtime_error("设置socket选项失败");
         }
     }//设置socket选项，例如SO_REUSEADDR，允许地址重用，如果设置失败则抛出异常。
    int set_nonblock() const
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags == -1)
            return -1;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }//将socket设置为非阻塞模式
    ssize_t recv(std::span<char> buffer, const int flags) const
    {
         ssize_t n = ::recv(fd, buffer.data(), buffer.size(), flags);
         return n;
    }//从socket接收数据，返回接收到的字节数，如果接收失败则返回-1。
    /*
     *线程池版函数，epoll版已废弃
    bool send_all(std::span<char> buffer, const size_t len) const
    {
         size_t sent = 0;
         while (sent < len)
         {
             ssize_t n = ::send(fd, buffer.data() + sent, len - sent, 0);
             if (n <= 0)
             {
                 return false;
             }
             sent += n;
         }
         return true;
    }//向socket发送数据，确保所有数据都被发送，如果发送失败则返回false。
    */
    ssize_t send(std::span<char> buffer, const int flags) const
    {
        ssize_t n = ::send(fd, buffer.data(), buffer.size(), flags);
        return n;
    }
    Socket accept()
    {
        sockaddr_in addr{};
        socklen_t addr_len = sizeof(addr);
        const int client_fd = ::accept(fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
        Socket client_socket(client_fd);
        client_socket.set_addr(addr);
        return client_socket;
    } //接受一个新的连接，并返回一个新的Socket对象。
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
    [[nodiscard]] std::string get_ip() const
    {
        return ip;
    }
    [[nodiscard]] uint16_t get_port() const
    {
        return ntohs(addr_.sin_port);
    }
    /*
     *线程池版本用函数，epoll版本已废弃
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
            ssize_t n = recv(buffer,0);
            if (n > 0)
            {
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << n << "bytes from " << ip << ":" << client_port << '\n' << std::string_view(buffer.data(),n) << std::endl;
                }
                send_all(buffer, n);
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
    */
    bool flush_output(std::string& out_buffer) const
    {
        while (!out_buffer.empty())
        {
            ssize_t n = send(out_buffer,0);
            if (n > 0)
            {
                out_buffer.erase(0,n);
            }
            else if (n==-1 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
                return true;
            }
            else if (n==-1 && errno == EINTR)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
        return true;
    }//将缓冲区数据发送给客户端
private:
    int fd;
    sockaddr_in addr_;
    std::string ip;
    socklen_t addr_len = sizeof(addr_);
    std::mutex cout_mutex;
};

#endif //UNTITLED10_SOCKET_H
