#include "net/client.hpp"
#include "net/io.hpp"
#include "net/json.hpp"
#include <readline/history.h>
#include <readline/readline.h>
#include <cstdlib>
#include <memory>
#include <optional>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>

namespace {
    // Options for the client
    struct ClientOptions{
        std::string_view address = "127.0.0.1";
        std::uint16_t port = 8080;
    };
    //get port from string
    std::uint16_t parse_port(std::string_view text){
        unsigned int port = 0;
        const auto [end,error] = std::from_chars(text.data(),text.data() + text.size(),port);
        if(error != std::errc{} || end != text.data() + text.size() || port > std::numeric_limits<std::uint16_t>::max() || port == 0){
            throw std::invalid_argument("Invalid port number: " + std::string(text));
        }
        return static_cast<std::uint16_t>(port);
    }
    ClientOptions parse_arguments(int argc,char* argv[]){
        if(argc > 3){
            throw std::invalid_argument("Usage: client [address] [port]");
        }
        ClientOptions op;
        if(argc >= 2){
            op.address = argv[1];
        }
        if(argc == 3){
            op.port = parse_port(argv[2]);
        }
        return op;
    }
    struct ReadlineBufferDeleter{
        void operator()(char* buffer) const noexcept{
            if(buffer != nullptr){
                std::free(buffer);
            }
        }
    };
    using ReadlineBuffer = std::unique_ptr<char,ReadlineBufferDeleter>;
    std::optional<std::string> read_command(){
        ReadlineBuffer buffer{::readline("minidb> ")};
        if(buffer == nullptr){
            return std::nullopt;
        }
        std::string command{buffer.get()};
        if(!command.empty()){
            ::add_history(command.c_str());
        }
        return command;
    }
}
int main(int argc,char* argv[]) {
    try {
        const ClientOptions options = parse_arguments(argc,argv);
        //connect to the server
        minidb::net::Socket connection = minidb::net::connect_to_server(options.address,options.port);
        std::cout << "Connected to server at " << options.address << ":" << options.port << std::endl;
        minidb::net::LineReader reader;
        const auto json = reader.read_line(connection.get_fd());
        if (!json.has_value()) {
            throw std::runtime_error(
                "server closed the connection before sending a welcome message"
            );
        }
        //show response from the server
        const minidb::net::Response response = minidb::net::json_to_response(json.value());
        std::cout << response.message << '\n';
        ::using_history();
        while(true){
            const std::optional<std::string> command = read_command();
            if(!command.has_value()){
                std::cout << "Exiting..." << std::endl;
                return 0;
            }
            if(command->empty()){
                continue;
            }
            const bool should_exit = (command.value() == "exit" || command.value() == "quit");
            //send command to the server
            const std::string frame = command.value() + '\n';
            minidb::net::send_all(connection.get_fd(),frame);
            //read response from the server
            const auto json_response = reader.read_line(connection.get_fd());
            if(!json_response.has_value()){
                throw std::runtime_error("server closed the connection");
            }
            const minidb::net::Response response = minidb::net::json_to_response(json_response.value());
            if(response.ok){
                std::cout << response.message << '\n';
            }else{
                std::cerr << "Error: " << response.message << '\n';
            }        
            if(should_exit){
                return 0;
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << "Client failed: " << exception.what() << '\n';
        return 1;
    }
}