#include "net/io.hpp"

#include <cerrno>
#include <stdexcept>
#include <system_error>
#include <sys/socket.h>
#include <array>

namespace minidb::net {
    void send_all(int socket_fd, std::string_view data) {   
        std::size_t total_sent = 0;
        while(total_sent < data.size()) {
            const ssize_t sent_len = ::send(socket_fd,data.data() + total_sent,data.size() - total_sent, MSG_NOSIGNAL);
            if(sent_len > 0){
                total_sent += static_cast<std::size_t>(sent_len);
                continue;
            }
            if(sent_len == -1 && errno == EINTR){
                continue; // interrupted, try again
            }
            if(sent_len == -1){
                throw std::system_error(errno,std::system_category(),"send failed");
            }
            throw std::runtime_error("send failed: sent 0 bytes, which should not happen");
        }
    }
    std::optional<std::string> LineReader::read_line(int socket_fd){
        while(true){
            //find '\n' as the end of line
            const std::size_t newline_pos = buffer_.find('\n');
            if(newline_pos != std::string::npos){
                std::string line = buffer_.substr(0,newline_pos);
                buffer_.erase(0,newline_pos + 1);
                return line;
            }
            std::array<char, 4096> temp_buffer{};
            const ssize_t received = ::recv(socket_fd,temp_buffer.data(),temp_buffer.size(),0);
            if(received > 0){
                buffer_.append(temp_buffer.data(),static_cast<std::size_t>(received));
                continue;
            }
            //disconnected
            if(received == 0){
                if(buffer_.empty()){
                    return std::nullopt;
                }
                throw std::runtime_error("socket closed before a complete line was received");
            }
            if(received == -1 && errno == EINTR){
                continue; // interrupted, try again
            }
            throw std::system_error(errno,std::generic_category(),"recv failed");
        }
    }
}