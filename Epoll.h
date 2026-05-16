//
// Created by MY on 2026/5/16.
//

#ifndef UNTITLED10_EPOLL_H
#define UNTITLED10_EPOLL_H

#include <stdexcept>
#include <unistd.h>
#include <sys/epoll.h>
class Epoll
{
public:
    Epoll();
    ~Epoll();
    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;
    Epoll(Epoll&& other) noexcept;
    Epoll& operator=(Epoll&& other) noexcept;
    void add(epoll_event& event) const;
    void modify(epoll_event& event) const;
    void del(epoll_event& event) const;
private:
    int epoll_fd;
};


#endif //UNTITLED10_EPOLL_H
