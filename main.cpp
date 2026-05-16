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

#include "Epoll.h"
#include "Socket.h"
using namespace std;

int main()
{
    signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号，防止服务器崩溃
    constexpr int PORT = 8080;
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
    listen_socket.set_nonblock();
    std::cout << "服务器已启动，监听端口 " << PORT << std::endl;
    Epoll epoll{};
    epoll.add(listen_socket.get_fd(),EPOLLIN);
    array<epoll_event,1024> events{};
    while (true)
    {
        int n = epoll.wait(events,-1);
        for (int i=0;i<n;++i)
        {
            int fd = events[i].data.fd;
            uint32_t ev = events[i].events;
            if (fd == listen_socket.get_fd())
            {
                epoll.accept_clients(listen_socket);
            }
            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                epoll.close_client(fd);
                continue;
            }
            if (ev & EPOLLIN)
            {
                epoll.handle_read(fd);
            }
            if (ev & EPOLLOUT)
            {
                epoll.handle_write(fd);
            }
        }
    }
}