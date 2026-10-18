#include "net/client.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <system_error>

namespace minidb::net{
    Socket connect_to_server(std::string_view address,std::uint16_t port){
        const int fd = ::socket(AF_INET,SOCK_STREAM,0);
        if(fd == -1){
            throw std::system_error(errno,std::generic_category(),"Failed to create socket");
        }
        //create the IP address to connect to
        Socket connection{fd};
        sockaddr_in server_address{};
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(port);
        const std::string address_text(address);
        const int conversion_res = ::inet_pton(AF_INET,address_text.c_str(),&server_address.sin_addr);
        if(conversion_res == 0){
            throw std::invalid_argument("Invalid IP address: " + address_text);
        }
        if(conversion_res == -1){
            throw std::system_error(errno,std::generic_category(),"Failed to convert IP address");
        }
        //connect to the server
        if(::connect(connection.get_fd(),reinterpret_cast<const sockaddr*>(&server_address),sizeof(server_address)) == -1){
            throw std::system_error(errno,std::generic_category(),"Failed to connect to server");
        }
        return connection;
    }
}