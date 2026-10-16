#include "net/socket.hpp"
#include <unistd.h>

namespace minidb::net {
    Socket::Socket(int fd) noexcept : fd_(fd) {}
    Socket::~Socket() {
        reset();
    }
    Socket::Socket(Socket&& other) noexcept : fd_(other.release()) {}
    Socket& Socket::operator=(Socket&& other) noexcept{
        if(this != &other){
            reset(other.release());
        }
        return *this;
    }
    int Socket::get_fd() const noexcept{
        return fd_;
    }
    bool Socket::is_valid() const noexcept{
        return fd_ >= 0;
    }
    int Socket::release() noexcept{
        const int old_fd = fd_;
        fd_ = -1;
        return old_fd;
    }
    void Socket::reset(int new_fd) noexcept{
        if(fd_ == new_fd){
            return;
        }
        if(is_valid()){
            ::close(fd_);
        }
        fd_ = new_fd;
    }
}