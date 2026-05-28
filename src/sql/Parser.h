#pragma once
#include "AST.h"
#include <string>
#include <stdexcept>
#include <cctype>

namespace dbms {

class Parser {
private:
    static std::string to_lower(const std::string& s) {
        std::string res = s;
        for (char& c : res) {
            c = std::tolower(static_cast<unsigned char>(c));
        }
        return res;
    }
    
    static Vector<std::string> tokenize(const std::string& sql) {
        Vector<std::string> tokens;
        std::string current;
        bool in_quotes = false;
        
        for (size_t i = 0; i < sql.length(); ++i) {
            char c = sql[i];
            if (c == '"') {
                in_quotes = !in_quotes;
                current += c;
            } else if (std::isspace(c) && !in_quotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
            } else if ((c == '(' || c == ')' || c == ',' || c == '=' || c == '<' || c == '>') && !in_quotes) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                tokens.push_back(std::string(1, c));
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        return tokens;
    }

    static Value parse_value(const std::string& token) {
        if (token.front() == '"' && token.back() == '"') {
            return Value(token.substr(1, token.length() - 2));
        }
        try {
            return Value(std::stoi(token));
        } catch (...) {
            throw std::runtime_error("Invalid value format: " + token);
        }
    }

    static void parse_where(const Vector<std::string>& tokens, size_t& i, SQLCommand& cmd) {
        if (i < tokens.size() && to_lower(tokens[i]) == "where") {
            i++;
            if (i + 2 >= tokens.size()) throw std::runtime_error("Incomplete WHERE clause");
            cmd.where_cond.column = to_lower(tokens[i++]);
            std::string op_str = tokens[i++];
            if (op_str == "=") cmd.where_cond.op = OpType::EQ;
            else if (op_str == "<") cmd.where_cond.op = OpType::LT;
            else if (op_str == ">") cmd.where_cond.op = OpType::GT;
            else throw std::runtime_error("Unsupported operator: " + op_str);
            cmd.where_cond.value = parse_value(tokens[i++]);
        }
    }

public:
    static SQLCommand parse(const std::string& sql) {
        SQLCommand cmd;
        Vector<std::string> tokens = tokenize(sql);
        if (tokens.empty()) return cmd;

        std::string first = to_lower(tokens[0]);

        if (first == "exit") {
            cmd.type = CommandType::EXIT;
            return cmd;
        }
        
        if (first == "show") {
            if (tokens.size() > 1 && (to_lower(tokens[1]) == "databases" || to_lower(tokens[1]) == "database")) {
                cmd.type = CommandType::SHOW_DATABASES;
                return cmd;
            } else {
                throw std::runtime_error("Syntax error in SHOW statement");
            }
        }

        if (first == "create") {
            if (tokens.size() < 3) throw std::runtime_error("Syntax error in CREATE statement");
            std::string second = to_lower(tokens[1]);
            if (second == "database") {
                cmd.type = CommandType::CREATE_DATABASE;
                cmd.db_name = to_lower(tokens[2]);
            } else if (second == "table") {
                cmd.type = CommandType::CREATE_TABLE;
                cmd.table_name = to_lower(tokens[2]);
                // parse columns
                size_t i = 3;
                if (i < tokens.size() && tokens[i] == "(") {
                    i++;
                    while (i < tokens.size() && tokens[i] != ")") {
                        std::string col_name = to_lower(tokens[i++]);
                        if (i >= tokens.size()) throw std::runtime_error("Unexpected end of column definition");
                        std::string type_str = to_lower(tokens[i++]);
                        DataType type;
                        if (type_str == "int") type = DataType::INT;
                        else if (type_str == "string") type = DataType::STRING;
                        else throw std::runtime_error("Unsupported data type: " + type_str);
                        
                        bool is_primary = false;
                        if (i < tokens.size() && to_lower(tokens[i]) == "primary") {
                            is_primary = true;
                            i++;
                        }
                        cmd.columns.push_back(Column(col_name, type, is_primary));
                        if (i < tokens.size() && tokens[i] == ",") i++;
                    }
                }
            } else {
                throw std::runtime_error("Unknown CREATE target");
            }
        } else if (first == "drop") {
            if (tokens.size() < 3) throw std::runtime_error("Syntax error in DROP statement");
            std::string second = to_lower(tokens[1]);
            if (second == "database") {
                cmd.type = CommandType::DROP_DATABASE;
                cmd.db_name = to_lower(tokens[2]);
            } else if (second == "table") {
                cmd.type = CommandType::DROP_TABLE;
                cmd.table_name = to_lower(tokens[2]);
            }
        } else if (first == "use") {
            if (tokens.size() < 2) throw std::runtime_error("Syntax error in USE statement");
            cmd.type = CommandType::USE_DATABASE;
            cmd.db_name = to_lower(tokens[1]);
        } else if (first == "insert") {
            // insert [into] table values (...)
            if (tokens.size() < 4) throw std::runtime_error("Syntax error in INSERT statement");
            size_t i = 1;
            if (to_lower(tokens[i]) == "into") i++; // optional 'into'
            cmd.type = CommandType::INSERT;
            cmd.table_name = to_lower(tokens[i++]);
            if (to_lower(tokens[i++]) != "values") throw std::runtime_error("Expected VALUES keyword");
            if (tokens[i++] != "(") throw std::runtime_error("Expected '(' in VALUES");
            while (i < tokens.size() && tokens[i] != ")") {
                cmd.insert_values.push_back(parse_value(tokens[i++]));
                if (i < tokens.size() && tokens[i] == ",") i++;
            }
        } else if (first == "delete") {
            if (tokens.size() < 3) throw std::runtime_error("Syntax error in DELETE statement");
            size_t i = 1;
            if (to_lower(tokens[i]) == "from") i++; // 'delete table' or 'delete from table' (MySQL allows 'delete from')
            cmd.type = CommandType::DELETE;
            cmd.table_name = to_lower(tokens[i++]);
            parse_where(tokens, i, cmd);
        } else if (first == "update") {
            if (tokens.size() < 6) throw std::runtime_error("Syntax error in UPDATE statement");
            cmd.type = CommandType::UPDATE;
            size_t i = 1;
            cmd.table_name = to_lower(tokens[i++]);
            if (to_lower(tokens[i++]) != "set") throw std::runtime_error("Expected SET keyword");
            cmd.update_column = to_lower(tokens[i++]);
            if (tokens[i++] != "=") throw std::runtime_error("Expected '=' in SET clause");
            cmd.update_value = parse_value(tokens[i++]);
            parse_where(tokens, i, cmd);
        } else if (first == "select") {
            if (tokens.size() < 4) throw std::runtime_error("Syntax error in SELECT statement");
            cmd.type = CommandType::SELECT;
            size_t i = 1;
            if (tokens[i] == "*") {
                cmd.select_columns.push_back("*");
                i++;
            } else {
                cmd.select_columns.push_back(to_lower(tokens[i++]));
                // To keep it simple, assume only one column or * (as per PDF: <column> | *)
            }
            if (to_lower(tokens[i++]) != "from") throw std::runtime_error("Expected FROM keyword");
            cmd.table_name = to_lower(tokens[i++]);
            parse_where(tokens, i, cmd);
        } else {
            throw std::runtime_error("Unknown statement: " + first);
        }

        return cmd;
    }
};

} // namespace dbms
