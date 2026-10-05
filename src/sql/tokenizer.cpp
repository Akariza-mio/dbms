#include "sql/tokenizer.hpp"
#include <string>
#include <cctype>

namespace minidb::sql{
    TokenizeError::TokenizeError(std::size_t pos,const std::string& msg)
        : std::runtime_error("tokenize error at position " + std::to_string(pos) + ": " + msg), pos_(pos){}
    std::size_t TokenizeError::get_pos() const noexcept{
        return pos_;
    }
    std::vector<Token> tokenize(std::string_view sql){
        std::vector<Token> tokens;
        std::size_t pos = 0;
        //get tokens only
        while(pos < sql.size()){
            const char c = sql[pos];
            //skip space,\n,tab.etc
            if(std::isspace(static_cast<unsigned char>(c))){
                ++pos;
                continue;
            }
            //scan TokenType::WORD
            if(std::isalpha(static_cast<unsigned char>(c)) || c == '_'){
                const std::size_t start = pos;
                ++pos;
                while(pos < sql.size()){
                    const char next = sql[pos];
                    if(!std::isalnum(static_cast<unsigned char>(next)) && next != '_'){
                        break;
                    }
                    ++pos;
                }
                tokens.push_back(Token{TokenType::WORD,std::string(sql.substr(start,pos - start)),start});
                continue;
            }
            //scan TokenType::NUMBER
            const bool starts_with_digit = std::isdigit(static_cast<unsigned char>(c));
            const bool starts_with_sign = (c == '+' || c == '-') && pos + 1 < sql.size() && std::isdigit(static_cast<unsigned char>(sql[pos + 1]));
            if(starts_with_digit || starts_with_sign){
                const std::size_t start = pos;
                if(sql[pos] == '+' || sql[pos] == '-'){
                    ++pos;
                }
                while(pos < sql.size() && std::isdigit(static_cast<unsigned char>(sql[pos]))){
                    ++pos;
                }
                if(pos < sql.size() && sql[pos] == '.'){
                    const std::size_t dot_pos = pos;
                    ++pos;
                    if(pos >= sql.size() || !std::isdigit(static_cast<unsigned char>(sql[pos]))){
                        throw TokenizeError(dot_pos,"expected digit after decimal point");
                    }
                    while(pos < sql.size() && std::isdigit(static_cast<unsigned char>(sql[pos]))){
                        ++pos;
                    }
                }
                tokens.push_back(Token{TokenType::NUMBER,std::string(sql.substr(start,pos - start)),start});
                continue;
            }
            //scan TokenType::STRING_LITERAL
            if(c == '\''){
                const std::size_t start = pos;
                ++pos;
                std::string value;
                bool closed = false;
                while(pos < sql.size()){
                    //two continous single quotes mean a single quote in string literal
                    if(sql[pos] == '\''){
                        if(pos + 1 < sql.size() && sql[pos + 1] == '\''){
                            value.push_back('\'');
                            pos += 2;
                            continue;
                        }
                        ++pos;
                        closed = true;
                        break;
                    }
                    value.push_back(sql[pos]);
                    ++pos;
                }
                if(!closed){
                    throw TokenizeError(start,"unclosed string literal");
                }
                tokens.push_back(Token{TokenType::STRING_LITERAL,value,start});
                continue;
            }
            TokenType type;
            switch(c){
                case '(':
                    type = TokenType::LEFT_PAREN;
                    break;
                case ')':
                    type = TokenType::RIGHT_PAREN;
                    break;
                case ',':
                    type = TokenType::COMMA;
                    break;
                case '=':
                    type = TokenType::EQUAL;
                    break;
                case '<':
                    type = TokenType::LESS;
                    break;
                case '>':
                    type = TokenType::GREATER;
                    break;
                case '*':
                    type = TokenType::ASTERISK;
                    break;
                case ';':
                    type = TokenType::SEMICOLON;
                    break;
                default:
                    throw TokenizeError(pos,std::string("invalid character '") + c + "'");
            }
            tokens.push_back(Token{type,std::string(1,c),pos});
            ++pos;
        }
        if(tokens.empty()){
            throw TokenizeError(0,"empty input");
        }
        //a mark for safety
        tokens.push_back(Token{TokenType::END_OF_INPUT,"",sql.size()});
        return tokens;
    }
}