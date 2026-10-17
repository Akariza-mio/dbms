#pragma once
#include "net/socket.hpp"
#include <cstdint>

namespace minidb::net{
    //create a socket bound to port and listening for connections
    [[nodiscard]] Socket create_tcp_listener(std::uint16_t port);
    //return new socket connected to the client
    [[nodiscard]] Socket accept_client(int listener_fd);
}