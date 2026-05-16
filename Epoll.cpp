//
// Created by MY on 2026/5/16.
//

#include "Epoll.h"
Epoll::Epoll()
{
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        throw std::runtime_error("epoll_create1 failed");
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
void Epoll::add(epoll_event& event) const
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event.data.fd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl failed");
    }
}
void Epoll::modify(epoll_event& event) const
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, event.data.fd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl failed");
    }
}
void Epoll::del(epoll_event& event) const
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event.data.fd, &event) == -1)
    {
        throw std::runtime_error("epoll_ctl failed");
    }
}