#include "sql/parser.hpp"
#include <string>
#include <cctype>
#include <utility>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace minidb::sql{
    namespace {
        //check if two strings are equal ignoring lower and upper case
        bool equals_ignore_case(std::string_view l,std::string_view r){
            if(l.size() != r.size()){
                return false;
            }
            for(std::size_t i = 0; i < l.size(); ++i){
                const unsigned char left_c = static_cast<unsigned char>(l[i]);
                const unsigned char right_c = static_cast<unsigned char>(r[i]);
                if(std::tolower(left_c) != std::tolower(right_c)){
                    return false;
                }
            }
            return true;
        }
    }
    ParserError::ParserError(std::size_t pos,const std::string& msg)
        : std::runtime_error("parser error at position " + std::to_string(pos) + ": " + msg),pos_(pos){}
    std::size_t ParserError::get_pos() const noexcept{
        return pos_;
    }
    Parser::Parser(std::string_view sql) : tokens_(tokenize(sql)){}
    const Token& Parser::current() const{
        if(now_index_ >= tokens_.size()){
            throw std::logic_error("parser token index is out of range");
        }
        return tokens_[now_index_];
    }
    bool Parser::check(TokenType type) const{
        return current().type == type;
    }
    const Token& Parser::advance(){
        const Token& token = current();
        if(token.type != TokenType::END_OF_INPUT){
            ++now_index_;
        }
        return token;
    }
    bool Parser::match(TokenType type){
        if(!check(type)){
            return false;
        }
        advance();
        return true;
    }
    const Token& Parser::expect(TokenType type,const std::string& msg){
        if(!check(type)){
            throw ParserError(current().pos,msg);
        }
        return advance();
    }
    bool Parser::check_keyword(std::string_view keyword) const{
        return check(TokenType::WORD) && equals_ignore_case(current().text,keyword);
    }
    bool Parser::match_keyword(std::string_view keyword){
        if(!check_keyword(keyword)){
            return false;
        }
        advance();
        return true;
    }
    void Parser::expect_keyword(std::string_view keyword){
        if(!check_keyword(keyword)){
            throw ParserError(current().pos,"expected keyword: '" + std::string(keyword) + "'");
        }
        advance();
    }
    //get the text of current WORD token and move to next token
    std::string Parser::expect_identifier(const std::string& description){
        const Token& token = expect(TokenType::WORD,"expected " + description);
        return token.text;
    }
    void Parser::expect_statement_end(){
        match(TokenType::SEMICOLON);
        if(!check(TokenType::END_OF_INPUT)){
            throw ParserError(current().pos,"unexpected token '" + current().text + "' after statement");
        }
    }
    // find the corresponding parse type
    SQLCommand Parser::parse(){
        if(match_keyword("CREATE")){
            return parse_create();
        }else if(match_keyword("DROP")){
            return parse_drop();
        }else if(match_keyword("USE")){
            return parse_use();
        }else if(match_keyword("SHOW")){
            return parse_show();
        }else if(match_keyword("ALTER")){
            return parse_alter();
        }else if(match_keyword("INSERT")){
            return parse_insert();
        }else if(match_keyword("DELETE")){
            return parse_delete();
        }else if(match_keyword("UPDATE")){
            return parse_update();
        }else if(match_keyword("SELECT")){
            return parse_select();
        }
        throw ParserError(current().pos,"unsupported SQL command '" + current().text + "'");
    }
    //CREATE DATABASE
    SQLCommand Parser::parse_create(){
        if(match_keyword("database")){
            SQLCommand command{};
            command.type = CommandType::CREATE_DATABASE;
            command.database_name = expect_identifier("database name");
            expect_statement_end();
            return command;
        }
        if(match_keyword("table")){
            return parse_create_table();
        }
        throw ParserError(current().pos,"expected 'database' or 'table' after 'CREATE'");
    }
    //DROP DATABASE
    SQLCommand Parser::parse_drop(){
        if(match_keyword("database")){
            SQLCommand command{};
            command.type = CommandType::DROP_DATABASE;
            command.database_name = expect_identifier("database name");
            expect_statement_end();
            return command;
        }
        if(match_keyword("table")){
            return parse_drop_table();
        }
        throw ParserError(current().pos,"expected 'database' or 'table' after 'DROP'");
    }
    //USE db_name
    SQLCommand Parser::parse_use(){
        SQLCommand command{};
        command.type = CommandType::USE_DATABASE;
        command.database_name = expect_identifier("database name");
        expect_statement_end();
        return command;
    }
    //SHOW DATABASES or TABLES
    SQLCommand Parser::parse_show(){
        SQLCommand command{};
        if(match_keyword("databases")){
            command.type = CommandType::SHOW_DATABASES;
        }else if(match_keyword("tables")){
            command.type = CommandType::SHOW_TABLES;
        }else{
            throw ParserError(current().pos,"expected 'databases' or 'tables' after 'SHOW'");
        }
        expect_statement_end();
        return command;
    }
    //parse data type
    DataType Parser::parse_data_type(){
        if(match_keyword("int")){
            return DataType::INT;
        }else if(match_keyword("float")){
            return DataType::FLOAT;
        }else if(match_keyword("string")){
            return DataType::STRING;
        }
        throw ParserError(current().pos,"expected data type 'int', 'float' or 'string'");
    }
    //CREATE TABLE
    SQLCommand Parser::parse_create_table(){
        SQLCommand command{};
        command.type = CommandType::CREATE_TABLE;
        command.table_name = expect_identifier("table name");
        // get '('
        expect(TokenType::LEFT_PAREN, "expected '(' after table name");
        if(check(TokenType::RIGHT_PAREN)){
            throw ParserError(current().pos,"expected at least one column definition");
        }
        bool has_primary_key = false;
        while(true){
            const std::size_t column_pos = current().pos;
            ColumnDefinition column{
                std::move(expect_identifier("column name")),
                parse_data_type(),
                false
            };
            //reject duplicate primary and column name
            if(match_keyword("primary")){
                if(has_primary_key){
                    throw ParserError(column_pos,"only one primary key is allowed");
                }
                if(column.type == DataType::FLOAT){
                    throw ParserError(column_pos,"primary key cannot be of type 'float'");
                }
                column.is_primary_key = true;
                has_primary_key = true;
            }
            for(const ColumnDefinition& col : command.columns){
                if(col.name == column.name){
                    throw ParserError(column_pos,"duplicate column name '" + column.name + "'");
                }
            }
            //end with comma or right parenthesis
            command.columns.push_back(std::move(column));
            if(match(TokenType::COMMA)){
                if(check(TokenType::RIGHT_PAREN)){
                    throw ParserError(current().pos,"expected column definition after ','");
                }
                continue;
            }
            expect(TokenType::RIGHT_PAREN,"expected ')' after column definitions");
            break;
        }
        expect_statement_end();
        return command;
    }
    //DROP TABLE
    SQLCommand Parser::parse_drop_table(){
        SQLCommand command{};
        command.type = CommandType::DROP_TABLE;
        command.table_name = expect_identifier("table name");
        expect_statement_end();
        return command;
    }
    //ALTER TABLE
    SQLCommand Parser::parse_alter(){
        expect_keyword("table");
        SQLCommand command{};
        command.type = CommandType::ALTER_TABLE;
        command.table_name = expect_identifier("table name");
        //add
        if(match_keyword("add")){
            const std::string column_name = expect_identifier("new column name");
            const DataType column_type = parse_data_type();
            command.alter_type = AlterType::ADD_COLUMN;
            command.alter_column = ColumnDefinition{std::move(column_name),column_type,false};
            expect_statement_end();
            return command;
        }
        //drop
        if(match_keyword("drop")){
            expect_keyword("column");
            command.alter_type = AlterType::DROP_COLUMN;
            command.target_column_name = expect_identifier("target column name");
            expect_statement_end();
            return command;
        }
        //rename 
        if(match_keyword("rename")){
            //rename table
            if(match_keyword("to")){
                command.alter_type = AlterType::RENAME_TABLE;
                command.new_name = expect_identifier("new table name");
                expect_statement_end();
                return command;
            }
            //rename column
            command.alter_type = AlterType::RENAME_COLUMN;
            command.target_column_name = expect_identifier("column name");
            expect_keyword("to");
            command.new_name = expect_identifier("new column name");
            expect_statement_end();
            return command;
        }
        throw ParserError(current().pos,"expected 'add', 'drop' or 'rename' after 'ALTER TABLE'");
    }
    Value Parser::parse_literal(){
        const Token& token = current();
        // parse string
        if(check(TokenType::STRING_LITERAL)){
            advance();
            return Value(token.text);
        }
        if(!check(TokenType::NUMBER)){
            throw ParserError(token.pos,"expected literal value");
        }
        advance();
        std::string_view text = token.text;
        if(!text.empty() && text.front() == '+'){
            text.remove_prefix(1);
        }
        //parse float
        if(token.text.find('.') != std::string::npos){
            double value{};
            const auto [end,error] = std::from_chars(text.data(),text.data() + text.size(),value);
            if(error == std::errc::result_out_of_range){
                throw ParserError(token.pos,"float literal value is out of range");
            }
            if(error != std::errc{} || end != text.data() + text.size()){
                throw ParserError(token.pos,"invalid float literal value");
            }
            return Value(value);
        }
        //parse integer
        std::int32_t value{};
        const auto [end,error] = std::from_chars(text.data(),text.data() + text.size(),value);
        if(error == std::errc::result_out_of_range){
            throw ParserError(token.pos,"integer literal value is out of range");
        }
        if(error != std::errc{} || end != text.data() + text.size()){
            throw ParserError(token.pos,"invalid integer literal value");
        }
        return Value(value);
    }
    SQLCommand Parser::parse_insert(){
        SQLCommand command{};
        command.type = CommandType::INSERT;
        //into is optional
        match_keyword("into");
        command.table_name = expect_identifier("table name");
        expect_keyword("values");
        expect(TokenType::LEFT_PAREN,"expected '(' after 'VALUES'");
        //VALUES () is not allowed
        if(check(TokenType::RIGHT_PAREN)){
            throw ParserError(current().pos,"expected at least one value in 'VALUES'");
        }
        while(true){
            command.values.push_back(parse_literal());
            if(match(TokenType::COMMA)){
                if(check(TokenType::RIGHT_PAREN)){
                    throw ParserError(current().pos,"expected value after ','");
                }
                continue;
            }
            expect(TokenType::RIGHT_PAREN,"expected ',' or ')' after values");
            break;
        }
        expect_statement_end();
        return command;
    }
    ComparisonOperator Parser::parse_comparison_op(){
        if(match(TokenType::EQUAL)){
            return ComparisonOperator::EQUAL;
        }
        if(match(TokenType::LESS)){
            return ComparisonOperator::LESS;
        }
        if(match(TokenType::GREATER)){
            return ComparisonOperator::GREATER;
        }
        throw ParserError(current().pos,"expected comparison operator");
    }
    //parse condition of WHERE clause
    Condition Parser::parse_condition(){
        std::string column_name = expect_identifier("column name");
        const ComparisonOperator cmp = parse_comparison_op();
        Value value = parse_literal();
        return Condition{std::move(column_name),cmp,std::move(value)};
    }
    SQLCommand Parser::parse_delete(){
        SQLCommand command{};
        command.type = CommandType::DELETE;
        //from is optional
        match_keyword("from");
        command.table_name = expect_identifier("table name");
        //where is optional
        parse_optional_where(command);
        expect_statement_end();
        return command;
    }
    SQLCommand Parser::parse_update(){
        SQLCommand command{};
        command.type = CommandType::UPDATE;
        command.table_name = expect_identifier("table name");
        //set is essential
        expect_keyword("set");
        while(true){
            std::string column_name = expect_identifier("column name");
            expect(TokenType::EQUAL,"expected '=' after column name");
            Value value = parse_literal();
            command.assignments.push_back(Assignment{std::move(column_name),std::move(value)});
            if(!match(TokenType::COMMA)){
                break;
            }
            //what comes after ',' must be another assignment
            if(check(TokenType::SEMICOLON) || check(TokenType::END_OF_INPUT) || check_keyword("where")){
                throw ParserError(current().pos,"expected assignment after ','");
            }
        }
        parse_optional_where(command);
        expect_statement_end();
        return command;
    }
    void Parser::parse_optional_where(SQLCommand& command){
        if(!match_keyword("where")){
            return;
        }
        //there must be at least one condition after WHERE
        command.conditions.push_back(parse_condition());
        while(true){
            if(match_keyword("and")){
                command.logical_operators.push_back(LogicalOperator::AND);
            }else if(match_keyword("or")){
                command.logical_operators.push_back(LogicalOperator::OR);
            }else{
                break;
            }
            command.conditions.push_back(parse_condition());
        }
    }
    SelectItem Parser::parse_selectItem(){
        // select *
        if(match(TokenType::ASTERISK)){
            return SelectItem{SelectType::ALL,"",AggregateType::NONE};
        }
        AggregateType aggregate = AggregateType::NONE;
        if(match_keyword("count")){
            aggregate = AggregateType::COUNT;
        }else if(match_keyword("max")){
            aggregate = AggregateType::MAX;
        }else if(match_keyword("min")){
            aggregate = AggregateType::MIN;
        }else if(match_keyword("avg")){
            aggregate = AggregateType::AVG;
        }
        //aggregate function is detected
        if(aggregate != AggregateType::NONE){
            expect(TokenType::LEFT_PAREN,"expected '(' after aggregate function");
            std::string column_name = expect_identifier("column name in aggregate function");
            expect(TokenType::RIGHT_PAREN,"expected ')' after column name");
            return SelectItem{SelectType::AGGREGATE,std::move(column_name),aggregate};
        }
        std::string column_name = expect_identifier("column name in select list");
        return SelectItem{SelectType::COLUMN,std::move(column_name),AggregateType::NONE};
    }
    SQLCommand Parser::parse_select(){
        SQLCommand command{};
        command.type = CommandType::SELECT;
        //FROM can not be the first token after SELECT
        if(check_keyword("from")){
            throw ParserError(current().pos,"expected select list before 'FROM'");
        }
        while(true){
            command.select_items.push_back(parse_selectItem());
            if(!match(TokenType::COMMA)){
                break;
            }
            if(check_keyword("from") || check(TokenType::SEMICOLON) || check(TokenType::END_OF_INPUT)){
                throw ParserError(current().pos,"expected select item after ','");
            }
        }
        expect_keyword("from");
        command.table_name = expect_identifier("table name");
        //where is optional
        parse_optional_where(command);
        //group by is optional
        if(match_keyword("group")){
            expect_keyword("by");
            command.group_by_column = expect_identifier("column name after 'GROUP BY'");
        }
        expect_statement_end();
        return command;
    }
}