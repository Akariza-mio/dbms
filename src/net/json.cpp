#include "net/json.hpp"
#include <stdexcept>
#include <cstddef>
#include <utility>

namespace minidb::net{
    namespace {
        //consume expected text
        void expect_text(std::string_view json,std::size_t& pos,std::string_view expected){
            if(json.substr(pos,expected.size()) != expected){
                throw std::runtime_error("Expected '" + std::string(expected) + "' at position " + std::to_string(pos));
            }
            pos += expected.size();
        }
        int hex_digit_value(char c){
            if(c >= '0' && c <= '9'){
                return c - '0';
            }
            if(c >= 'a' && c <= 'f'){
                return 10 + (c - 'a');
            }
            if(c >= 'A' && c <= 'F'){
                return 10 + (c - 'A');
            }
            throw std::runtime_error("Invalid hex digit in json escape");
        }
        std::string read_json_string(std::string_view json,std::size_t& pos){
            expect_text(json,pos,"\"");
            std::string res;
            while(pos < json.size()){
                const char c = json[pos++];
                if(c == '"'){
                    return res;
                }
                if(c != '\\'){
                    if(static_cast<unsigned char>(c) < 0x20){
                        throw std::runtime_error("Invalid control character in json string");
                    }
                    res.push_back(c);
                    continue;
                }
                if(pos >= json.size()){
                    throw std::runtime_error("Unexpected end of input in json string escape");
                }
                const char escaped = json[pos++];
                switch(escaped){
                    case '"':
                        res.push_back('"');
                        break;
                    case '\\':
                        res.push_back('\\');
                        break;
                    case '/':
                        res.push_back('/');
                        break;
                    case 'b':
                        res.push_back('\b');
                        break;
                    case 'f':
                        res.push_back('\f');
                        break;
                    case 'n':
                        res.push_back('\n');
                        break;
                    case 'r':   
                        res.push_back('\r');
                        break;
                    case 't':
                        res.push_back('\t');
                        break;
                    case 'u':{
                        if(pos + 4 > json.size()){
                            throw std::runtime_error("Unexpected end of input in json unicode escape");
                        }
                        unsigned int codepoint = 0;
                        for(int i = 0; i < 4; ++i){
                            codepoint = codepoint * 16 + static_cast<unsigned int>(hex_digit_value(json[pos + i]));
                        }
                        pos += 4;
                        if(codepoint > 0x7F){
                            throw std::runtime_error("Only ASCII characters are supported in json string");
                        }
                        res.push_back(static_cast<char>(codepoint));
                        break;
                    }
                    default:
                        throw std::runtime_error("Invalid escape character in json string");
                }
            }
            throw std::runtime_error("unclosed json string");
        }
    }
    std::string to_json_string(std::string_view text){
        std::string res;
        res.reserve(text.size() + 2);
        res.push_back('"');
        constexpr char HEX_DIGITS[] = "0123456789abcdef";
        for(char c : text){
            switch(c){
                case '"':
                    res += "\\\"";
                    break;
                case '\\':
                    res += "\\\\";
                    break;
                case '\b':
                    res += "\\b";
                    break;
                case '\f':
                    res += "\\f";
                    break;
                case '\n':
                    res += "\\n";
                    break;
                case '\r':
                    res += "\\r";
                    break;
                case '\t':
                    res += "\\t";
                    break;
                default:{
                    const unsigned char uc = static_cast<unsigned char>(c);
                    if(uc < 0x20){
                        res += "\\u00";
                        res.push_back(HEX_DIGITS[(uc >> 4) & 0x0F]);
                        res.push_back(HEX_DIGITS[uc & 0x0F]);
                    }else{
                        res.push_back(c);
                    }
                    break;
                }
            }
        }
        res.push_back('"');
        return res;
    }
    std::string to_json(const Response& response){
        std::string res = "{\"ok\":";
        res += response.ok ? "true" : "false";
        res += ",\"msg\":";
        res += to_json_string(response.message);
        res.push_back('}');
        return res;
    }
    Response json_to_response(std::string_view json){
        std::size_t pos = 0;
        expect_text(json,pos,"{\"ok\":");
        bool ok;
        if(json.substr(pos,4) == "true"){
            ok = true;
            pos += 4;
        }else if(json.substr(pos,5) == "false"){
            ok = false;
            pos += 5;
        }else{
            throw std::runtime_error("Expected 'true' or 'false' for 'ok' field in json response");
        }
        expect_text(json,pos,",\"msg\":");
        std::string message = read_json_string(json,pos);
        expect_text(json,pos,"}");
        if(pos != json.size()){
            throw std::runtime_error("Unexpected characters after end of json response");
        }
        return Response{ok,std::move(message)};
    }
}