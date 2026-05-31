#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "sql/Parser.h"
#include "sql/Executor.h"
#include "storage/JsonSerializer.h"

using namespace dbms;

std::mutex db_mutex;

void handle_client(int client_sock) {
    Executor executor; 
    char buffer[4096];
    
    std::string greeting = JsonWriter::ok("Welcome to MiniDBMS!") + "\n";
    send(client_sock, greeting.c_str(), greeting.length(), 0);
    
    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        int bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }
        
        std::string sql(buffer);
        if (!sql.empty() && sql.back() == '\n') sql.pop_back();
        if (!sql.empty() && sql.back() == '\r') sql.pop_back();
        
        if (sql.empty()) continue;
        
        std::cout << "Received SQL: " << sql << std::endl;
        
        std::string response;
        try {
            SQLCommand cmd = Parser::parse(sql);
            if (cmd.type == CommandType::EXIT) {
                response = JsonWriter::ok("Goodbye!") + "\n";
                send(client_sock, response.c_str(), response.length(), 0);
                break;
            }

            std::lock_guard<std::mutex> lock(db_mutex);
            response = executor.execute(cmd);
            response = JsonWriter::ok(response);
        } catch (const std::exception& e) {
            response = JsonWriter::error(e.what());
        }

        response += "\n";
        send(client_sock, response.c_str(), response.length(), 0);
    }
    
    close(client_sock);
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::cerr << "Failed to create socket." << std::endl;
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind." << std::endl;
        return 1;
    }
    
    if (listen(server_sock, 5) < 0) {
        std::cerr << "Failed to listen." << std::endl;
        return 1;
    }
    
    std::cout << "MiniDBMS Server listening on port " << port << "..." << std::endl;
    
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_sock < 0) {
            std::cerr << "Failed to accept client." << std::endl;
            continue;
        }
        
        std::cout << "New client connected from " << inet_ntoa(client_addr.sin_addr) << std::endl;
        
        std::thread client_thread(handle_client, client_sock);
        client_thread.detach();
    }
    
    close(server_sock);
    return 0;
}
