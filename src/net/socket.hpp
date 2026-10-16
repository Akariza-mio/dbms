#pragma once

namespace minidb::net{
class Socket{
public:
    //create a socket without fd
    Socket() noexcept = default;
    //take ownership of fd
    explicit Socket(int fd) noexcept;
    //destructor
    ~Socket();
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    // move constructor
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    [[nodiscard]] int get_fd() const noexcept;
    //if fd is valid
    [[nodiscard]] bool is_valid() const noexcept;
    // release ownership of fd and return it
    [[nodiscard]] int release() noexcept;
    void reset(int new_fd = -1) noexcept;
private:
    int fd_ = -1;
};
}