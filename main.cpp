#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
using namespace std;

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
    int listen_fd = socket(AF_INET,SOCK_STREAM,0);
    if (listen_fd == -1)
    {
        perror("socket");
        return -1;
    }
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listen_fd,reinterpret_cast<sockaddr*>(&server_addr),sizeof(server_addr)) == -1)
    {
        perror("bind");
        close(listen_fd);
        return -1;
    }
    if (listen(listen_fd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(listen_fd);
        return -1;
    }
    while (true)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd,reinterpret_cast<sockaddr*>(&client_addr),&client_len);
        if (client_fd == -1)
        {
            perror("accept");
            close(listen_fd);
            return -1;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(
            AF_INET,
            &client_addr.sin_addr,
            client_ip,
            sizeof(client_ip)
        );
        cout << "Client connected: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
        char buffer[4096];
        while (true)
        {
            ssize_t n = recv(client_fd, buffer, sizeof(buffer), 0);
            if (n>0)
            {
                cout << "Received from client: " << string_view(buffer,n) << endl;
                if (!send_all(client_fd,buffer,n))
                {
                    cerr << "Failed to send data to client" << endl;
                    close(client_fd);
                    break;
                }
            }
            else if (n==0)
            {
                cout << "Client disconnected: " << client_ip << ":" << ntohs(client_addr.sin_port) << endl;
                close(client_fd);
                break;
            }
            else
            {
                perror("recv");
                close(client_fd);
                break;
            }
        }
        close(client_fd);
    }
}