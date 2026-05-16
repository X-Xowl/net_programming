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
#include "Socket.h"
using namespace std;

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