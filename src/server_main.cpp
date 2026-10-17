#include "net/server.hpp"
#include "net/io.hpp"
#include "net/json.hpp"
#include "execution/executor.hpp"
#include "sql/parser.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <filesystem>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
    std::string format_execution_result(const minidb::ExecutionResult& result){
        // DDL/DML has no table-shaped result, so its message is enough.
        if(result.columns.empty()){
            return result.message;
        }
        std::vector<std::size_t> column_widths;
        column_widths.reserve(result.columns.size());
        for(const std::string& column : result.columns){
            column_widths.push_back(column.size());
        }
        // Each column must be wide enough for its name and all its values.
        for(const auto& row : result.rows){
            if(row.size() != result.columns.size()){
                throw std::logic_error("execution result column count mismatch");
            }
            for(std::size_t i = 0; i < row.size(); ++i){
                column_widths[i] = std::max(column_widths[i],row[i].size());
            }
        }
        std::ostringstream output;
        const auto write_separator = [&output,&column_widths](){
            output << '+';
            for(const std::size_t width : column_widths){
                output << std::string(width + 2,'-') << '+';
            }
            output << '\n';
        };
        const auto write_row = [&output,&column_widths](const std::vector<std::string>& cells){
            output << '|';
            for(std::size_t i = 0; i < cells.size(); ++i){
                output << ' ' << cells[i];
                output << std::string(column_widths[i] - cells[i].size(),' ');
                output << " |";
            }
            output << '\n';
        };
        // Draw a top line and another line below the column names.
        write_separator();
        write_row(result.columns);
        write_separator();
        for(const auto& row : result.rows){
            write_row(row);
        }
        output << result.message;
        return output.str();
    }

    //send a response to the client in json
    void send_response(int fd,const minidb::net::Response& response){
        std::string frame = minidb::net::to_json(response) + '\n';
        minidb::net::send_all(fd,frame);
    }
    void handle_client(minidb::net::Socket client,const std::filesystem::path& data_directory){
        const int fd = client.get_fd();
        //executor
        minidb::Executor executor(data_directory);
        //hello
        send_response(fd,minidb::net::Response{true,"Welcome to MiniDBMS!"});
        minidb::net::LineReader line_reader;
        //keep reading messages from the client
        while(true){
            const std::optional<std::string> line = line_reader.read_line(client.get_fd());
            if(!line.has_value()){
                std::cout << "Client disconnected, socket fd = " << client.get_fd() << std::endl;
                return;
            }
            if(line.value() == "exit" || line.value() == "quit"){
                send_response(fd,minidb::net::Response{true,"Goodbye!"});
                return;
            }
            minidb::net::Response response{false,""};
            //parse and execute the SQL command
            try{
                minidb::sql::Parser parser(line.value());
                minidb::sql::SQLCommand command = parser.parse();
                minidb::ExecutionResult result = executor.execute(command);
                response.ok = true;
                response.message = format_execution_result(result);
            }catch(const std::exception& error){
                response.ok = false;
                response.message = error.what();
            }
            send_response(fd,response);
        }
    }
    std::uint16_t parse_port(int argc,char* argv[]){
        constexpr std::uint16_t default_port = 8080;
        if(argc == 1){
            return default_port;
        }
        if(argc != 2){
            throw std::invalid_argument("Usage: server [port]");
        }
        const std::string_view text(argv[1]);
        unsigned int port = 0;
        const auto [end,error] = std::from_chars(text.data(),text.data() + text.size(),port);
        if(error != std::errc{} || end != text.data() + text.size() || port > std::numeric_limits<std::uint16_t>::max() || port == 0){
            throw std::invalid_argument("Invalid port number: " + std::string(text));
        }
        return static_cast<std::uint16_t>(port);
    }
}
int main(int argc,char* argv[]) {
    try {
        const std::uint16_t port = parse_port(argc,argv);
        const std::filesystem::path data_directory = "data";
        //create listener
        minidb::net::Socket listener = minidb::net::create_tcp_listener(port);
        std::cout
            << "MiniDBMS server is listening on 127.0.0.1:"
            << port
            << std::endl;
        //keep listening and accepting clients
        while(true){
            minidb::net::Socket client = minidb::net::accept_client(listener.get_fd());
            std::cout << "Client connected, socket fd = " << client.get_fd() << std::endl;
            try{
                handle_client(std::move(client),data_directory);
            } catch (const std::exception& exception) {
                std::cerr << "Error handling client: " << exception.what() << '\n';
            }
        }
    } catch (const std::exception& exception) {
        std::cerr << "Server failed: " << exception.what() << '\n';
        return 1;
    }
}
