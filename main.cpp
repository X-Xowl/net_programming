#include <iostream>
#include <string>
#include <string_view>
#include <array>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;
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
    // 禁止复制和移动构造函数和赋值运算符，确保Socket对象的唯一性。
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& other) = delete;
    Socket& operator=(Socket&& other) = delete;
     ~Socket()
    {
        if (fd != -1)
        {
            close(fd);
        }
    }
    [[nodiscard]] int get_fd() const
    {
        return fd;
    }
    void bind(sockaddr_in& addr) const
     {
         if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
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
    }
    void setsockopt(int level,int optname, int& opt) const
     {
         if (::setsockopt(fd, level, optname, &opt, sizeof(opt)) == -1)
         {
             throw runtime_error("Failed to set socket options");
         }
     }
    ssize_t recv(char* buffer,int len,int flags) const
    {
         ssize_t n = ::recv(fd, buffer, len, flags);
         return n;
    }
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
    }
private:
    int fd;
};

int main()
{
    const Socket listen_socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    listen_socket.setsockopt(SOL_SOCKET,SO_REUSEADDR,opt);
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_socket.bind(server_addr);
    listen_socket.listen(SOMAXCONN);
    // ReSharper disable once CppDFAEndlessLoop
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        const int client_fd = accept(listen_socket.get_fd(),reinterpret_cast<sockaddr*>(&client_addr),&client_len);
        Socket client_socket(client_fd);
        array<char,INET_ADDRSTRLEN> client_ip{};
        inet_ntop(
            AF_INET,
            &client_addr.sin_addr,
            client_ip.data(),
            client_ip.size()
        );
        cout << "Client connected: " << string_view(client_ip.data()) << ":" << ntohs(client_addr.sin_port) << endl;
        array<char,4096> buffer{};
        while (true)
        {
            ssize_t n = client_socket.recv(buffer.data(),buffer.size(),0);
            if (n>0)
            {
                cout << "Received from client: " << string_view(buffer.data(),n) << endl;
                if (client_socket.send_all(buffer.data(),n))
                {
                    cout << "Sent data back to client" << endl;
                }
                else
                {
                    cerr << "Failed to send data to client" << endl;
                    break;
                }
            }
            else if (n==0)
            {
                cout << "Client disconnected: " << string_view(client_ip.data()) << ":" << ntohs(client_addr.sin_port) << endl;
                break;
            }
            else
            {
                perror("recv");
                break;
            }
        }
    }
}