#pragma once
#include "sql/ast.hpp"
#include "sql/tokenizer.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace minidb::sql{
class ParserError final : public std::runtime_error{
public:
    ParserError(std::size_t pos,const std::string& msg);
    [[nodiscard]] std::size_t get_pos() const noexcept;
private:
    std::size_t pos_;
}; 
class Parser{
public:
    explicit Parser(std::string_view sql);
    [[nodiscard]] SQLCommand parse();
private:
    std::vector<Token> tokens_;
    std::size_t now_index_ = 0;
    //get current token
    [[nodiscard]] const Token& current() const;
    //check if current token is of the given type
    [[nodiscard]] bool check(TokenType type) const;
    //get current token and move to next token
    const Token& advance();
    //return true when tokentype matches and move to next token,
    //otherwise return false
    bool match(TokenType type);
    //expect current token to be the given type,
    //if not, throw ParserError
    const Token& expect(TokenType type,const std::string& msg);
    
    //same as word
    [[nodiscard]] bool check_keyword(std::string_view keyword) const;
    bool match_keyword(std::string_view keyword);
    void expect_keyword(std::string_view keyword);
    [[nodiscard]] std::string expect_identifier(const std::string& description);
    
    //check semicolon or end_of_input
    void expect_statement_end();
    // parse SQL of database and table
    [[nodiscard]] SQLCommand parse_create();
    [[nodiscard]] SQLCommand parse_drop();
    [[nodiscard]] SQLCommand parse_use();
    [[nodiscard]] SQLCommand parse_show();
    [[nodiscard]] DataType parse_data_type();
    [[nodiscard]] SQLCommand parse_create_table();
    [[nodiscard]] SQLCommand parse_drop_table();
    [[nodiscard]] SQLCommand parse_alter();
    //turn literal token into value in row
    [[nodiscard]] Value parse_literal();
    //parsen DML
    [[nodiscard]] SQLCommand parse_insert();
    [[nodiscard]] ComparisonOperator parse_comparison_op();
    [[nodiscard]] Condition parse_condition();
    [[nodiscard]] SQLCommand parse_delete();
};
}
