#include <csignal>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <array>
#include <memory>
#include "ThreadPool.h"
using namespace std;
mutex cout_mutex;
//Socket类，封装了socket的创建、绑定、监听、接受连接等操作，简化了代码的编写。
class Socket
{
public:
    Socket(const int domain, const int type, const int protocol)
    {
        fd = socket(domain, type, protocol);
        if (fd == -1)
        {
            throw runtime_error("Failed to create socket");
        }
    }

    explicit Socket(const int fd) : fd(fd)
    {
        if (fd == -1)
        {
            throw runtime_error("Invalid socket file descriptor");
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
            lock_guard<mutex> lock(cout_mutex);
            cout << "socket closed: "
                 << ip << ":"
                 << ntohs(addr_.sin_port) << endl;
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
             throw runtime_error("Failed to bind socket");
         }
     } //将socket绑定到指定的地址和端口上，如果绑定失败则抛出异常。
    void listen(const int backlog) const
    {
        if (::listen(fd, backlog) == -1)
        {
            throw runtime_error("Failed to listen on socket");
        }
    }//将socket设置为监听状态，等待客户端连接，如果监听失败则抛出异常。
    void setsockopt(const int level,const int optname, int& opt) const
     {
         if (::setsockopt(fd, level, optname, &opt, sizeof(opt)) == -1)
         {
             throw runtime_error("Failed to set socket options");
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
            throw runtime_error("Failed to accept connection");
        }
        Socket client_socket(client_fd);
        client_socket.set_addr(addr);
        return client_socket;
    } //接受一个新的连接，并返回一个新的Socket对象，如果接受失败则抛出异常。
    void set_addr(const sockaddr_in& addr)
    {
        addr_ = addr;
        array<char,INET_ADDRSTRLEN> ip_{};
        inet_ntop(
            AF_INET,
            &addr_.sin_addr,
            ip_.data(),
            ip_.size()
        );
        ip = string(ip_.data());
    }
    sockaddr_in get_addr()
    {
        return addr_;
    }
    void handle_client()
    {
        auto client_port = ntohs(addr_.sin_port);
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "client connected: "
                 << ip << ":"
                 << client_port << '\n'
                 << "thread id: " << this_thread::get_id() << endl;
        }
        array<char,4096> buffer{};
        while (true)
        {
            ssize_t n = recv(buffer.data(),buffer.size(),0);
            if (n > 0)
            {
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << n << "bytes from " << ip << ":" << client_port << '\n' << string_view(buffer.data(),n) << endl;
                }
                send_all(buffer.data(), n);
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "echoed back to " << ip << ":" << client_port << endl;
                }
            }
            else if (n == 0)
            {
                lock_guard<mutex> lock(cout_mutex);
                cout  << "client disconnected: " << ip << ":" << client_port << endl;
                break;
            }
            else
            {
                if (errno == EINTR)
                {
                    continue;
                }
                lock_guard<mutex> lock(cout_mutex);
                cerr << "recv failed from "
                     << ip << ":" << client_port
                     << " with error: " << strerror(errno) << endl;
                break;
            }
        }
    }
private:
    int fd;
    sockaddr_in addr_;
    string ip;
    socklen_t addr_len = sizeof(addr_);
};

int main()
{
    signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号，防止服务器崩溃
    constexpr int PORT = 8080;
    ThreadPool pool(4);
    Socket listen_socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    listen_socket.setsockopt(SOL_SOCKET,SO_REUSEADDR,opt);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_socket.set_addr(server_addr);
    listen_socket.bind();
    listen_socket.listen(SOMAXCONN);
    cout << "threads pool echo server listening on port " << PORT << endl;
    // ReSharper disable once CppDFAEndlessLoop
    while (true)
    {
        auto client_socket_ptr = make_shared<Socket>(listen_socket.accept());//接受一个连接，并把地址信息写入client_addr
        auto client_addr = client_socket_ptr->get_addr();
        array<char,INET_ADDRSTRLEN> client_ip{};
        inet_ntop(
        AF_INET,
        &client_addr.sin_addr,
        client_ip.data(),
        client_ip.size()
        );
        cout << "Client connected: " << string_view(client_ip.data()) << ":" << ntohs(client_addr.sin_port) << endl;
        bool ok = pool.enqueue([client_socket_ptr]()
        {
            client_socket_ptr->handle_client();
        });
        if (!ok)
        {
            cerr << "Failed to enqueue task for client: " << string_view(client_ip.data()) << ":" << ntohs(client_addr.sin_port) << endl;
        }
    }
}