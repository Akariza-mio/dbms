#pragma once
#include "net/socket.hpp"
#include <cstdint>
#include <string_view>

namespace minidb::net{
    [[nodiscard]] Socket connect_to_server(std::string_view address,std::uint16_t port);
}