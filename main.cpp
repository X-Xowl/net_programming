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
private:
    int fd;
};
inline bool send_all(const int fd, const char* data, const size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd,data + sent,len - sent,0);
        if (n <= 0)
        {
            return false;
        }
        sent += n;
    }
    return true;
}

int main()
{
    Socket listen_socket(AF_INET,SOCK_STREAM,0);
    int listen_fd = listen_socket.get_fd();
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        return -1;
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd,reinterpret_cast<sockaddr*>(&server_addr),sizeof(server_addr)) == -1)
    {
        perror("bind");
        return -1;
    }
    if (listen(listen_fd, SOMAXCONN) == -1)
    {
        perror("listen");
        return -1;
    }
    // ReSharper disable once CppDFAEndlessLoop
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd,reinterpret_cast<sockaddr*>(&client_addr),&client_len);
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
            ssize_t n = recv(client_fd, buffer.data(), sizeof(buffer), 0);
            if (n>0)
            {
                cout << "Received from client: " << string_view(buffer.data(),n) << endl;
                if (!send_all(client_fd,buffer.data(),n))
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