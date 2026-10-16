#pragma once
#include <string>
#include <string_view>

namespace minidb::net{
    struct Response{
        bool ok;
        std::string message;
    };
    //convert string to string that can be put into JSON
    [[nodiscard]] std::string to_json_string(std::string_view text);
    // convert response to JSON
    [[nodiscard]] std::string to_json(const Response& response);
    // convert JSON to response
    [[nodiscard]] Response json_to_response(std::string_view json);
}