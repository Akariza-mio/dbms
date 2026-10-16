#pragma once
#include <optional>
#include <string>
#include <string_view>

namespace minidb::net{
    //send all bytes in data
    void send_all(int socket_fd,std::string_view data);
    //read a message line until there is a '\n' or EOF
    class LineReader{
    public:
        [[nodiscard]] std::optional<std::string> read_line(int socket_fd);
    private:
        std::string buffer_;
    };
}
