#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "storage/JsonSerializer.h"

using namespace dbms;

extern "C" {
#include "linenoise.h"
}

int main(int argc, char* argv[]) {
    std::string ip = "127.0.0.1";
    int port = 8080;
    
    if (argc > 1) ip = argv[1];
    if (argc > 2) port = std::stoi(argv[2]);
    
    std::cout << "Connecting to " << ip << ":" << port << "..." << std::endl;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address." << std::endl;
        return 1;
    }
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        return 1;
    }
    
    char buffer[8192];
    std::memset(buffer, 0, sizeof(buffer));
    int bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        std::string raw(buffer);
        JsonReader jr(raw);
        std::cout << jr.get_string("msg") << std::endl;
    }
    
    while (true) {
        char* input = linenoise("> ");
        if (!input) {
            std::cout << std::endl;
            break;
        }
        
        std::string sql(input);
        if (!sql.empty()) {
            linenoiseHistoryAdd(input);
        }
        free(input);
        
        if (sql.empty()) continue;
        
        send(sock, sql.c_str(), sql.length(), 0);
        
        if (sql == "exit" || sql == "quit") break;
        
        std::memset(buffer, 0, sizeof(buffer));
        bytes_read = recv(sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes_read <= 0) {
            std::cout << "Server disconnected." << std::endl;
            break;
        }

        std::string raw(buffer);
        JsonReader jr(raw);
        std::cout << jr.get_string("msg") << std::endl;
    }
    
    close(sock);
    return 0;
}
