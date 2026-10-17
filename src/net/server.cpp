#include "net/server.hpp"
#include <cerrno>
#include <system_error>
#include <netinet/in.h>
#include <sys/socket.h>

namespace minidb::net{
    Socket create_tcp_listener(std::uint16_t port){
        const int fd = ::socket(AF_INET,SOCK_STREAM,0);
        if(fd == -1){
            throw std::system_error(errno,std::generic_category(),"socket creation failed");
        }
        Socket listener{fd};
        const int reuse_address = 1;
        if(::setsockopt(listener.get_fd(),SOL_SOCKET,SO_REUSEADDR,&reuse_address,sizeof(reuse_address)) == -1){
            throw std::system_error(errno,std::generic_category(),"setsockopt failed");
        }
        //prepare IPv4 address
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        if(::bind(listener.get_fd(),reinterpret_cast<const sockaddr*>(&address),sizeof(address)) == -1){
            throw std::system_error(errno,std::generic_category(),"bind failed");
        }
        constexpr int backlog = 16;
        if(::listen(listener.get_fd(),backlog) == -1){
            throw std::system_error(errno,std::generic_category(),"listen failed");
        }
        return listener;
    }
    Socket accept_client(int listener_fd){
        // get new connected socket 
        while(true){
            const int client_fd = ::accept(listener_fd,nullptr,nullptr);
            if(client_fd >= 0){
                return Socket{client_fd};
            }
            if(errno == EINTR){
                continue; // interrupted, try again
            }
            throw std::system_error(errno,std::generic_category(),"accept failed");
        }
    }
}
