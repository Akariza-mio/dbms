#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace minidb::sql {
    enum class TokenType{
        //key word
        WORD,
        //number
        NUMBER,
        // 'name'
        STRING_LITERAL,
        // ( )
        LEFT_PAREN,
        RIGHT_PAREN,
        // ,
        COMMA,
        EQUAL,
        LESS,
        GREATER,
        // *
        ASTERISK,
        // ;
        SEMICOLON,
        END_OF_INPUT
    };
    struct Token{
        TokenType type;
        std::string text;
        std::size_t pos;
    };
class TokenizeError : public std::runtime_error{
public:
    TokenizeError(const std::size_t pos,const std::string& msg);
    [[nodiscard]] std::size_t get_pos() const noexcept;
private:
    std::size_t pos_;
};
    [[nodiscard]] std::vector<Token> tokenize(std::string_view sql);
}