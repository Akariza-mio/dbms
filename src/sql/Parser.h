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
            if (token.find('.') != std::string::npos) {
                return Value(std::stod(token));
            }
            return Value(std::stoi(token));
        } catch (...) {
            throw std::runtime_error("Invalid value format: " + token);
        }
    }

    static void parse_where(const Vector<std::string>& tokens, size_t& i, SQLCommand& cmd) {
        if (i < tokens.size() && to_lower(tokens[i]) == "where") {
            i++;
            while (i < tokens.size()) {
                WhereCondition cond;
                cond.column = to_lower(tokens[i++]);
                if (i >= tokens.size()) throw std::runtime_error("Incomplete WHERE clause");
                std::string op_str = tokens[i++];
                if (op_str == "=") cond.op = OpType::EQ;
                else if (op_str == "<") cond.op = OpType::LT;
                else if (op_str == ">") cond.op = OpType::GT;
                else throw std::runtime_error("Unsupported operator: " + op_str);
                
                if (i >= tokens.size()) throw std::runtime_error("Incomplete WHERE clause");
                cond.value = parse_value(tokens[i++]);
                
                if (i < tokens.size()) {
                    std::string logic = to_lower(tokens[i]);
                    if (logic == "and") {
                        cond.next_logic = LogicalOp::AND;
                        i++;
                    } else if (logic == "or") {
                        cond.next_logic = LogicalOp::OR;
                        i++;
                    } else {
                        cmd.where_conds.push_back(cond);
                        break;
                    }
                }
                cmd.where_conds.push_back(cond);
            }
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
            } else if (tokens.size() > 1 && (to_lower(tokens[1]) == "tables" || to_lower(tokens[1]) == "table")) {
                cmd.type = CommandType::SHOW_TABLES;
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
                        else if (type_str == "float") type = DataType::FLOAT;
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
            while (i < tokens.size() && to_lower(tokens[i]) != "where") {
                cmd.update_columns.push_back(to_lower(tokens[i++]));
                if (i >= tokens.size() || tokens[i++] != "=") throw std::runtime_error("Expected '=' in SET clause");
                if (i >= tokens.size()) throw std::runtime_error("Incomplete SET clause");
                cmd.update_values.push_back(parse_value(tokens[i++]));
                if (i < tokens.size() && tokens[i] == ",") i++;
                else if (i < tokens.size() && to_lower(tokens[i]) != "where") throw std::runtime_error("Syntax error in SET clause");
            }
            parse_where(tokens, i, cmd);
        } else if (first == "select") {
            if (tokens.size() < 4) throw std::runtime_error("Syntax error in SELECT statement");
            cmd.type = CommandType::SELECT;
            size_t i = 1;
            if (tokens[i] == "*") {
                SelectItem item; item.column = "*";
                cmd.select_items.push_back(item);
                i++;
            } else {
                while (i < tokens.size() && to_lower(tokens[i]) != "from") {
                    if (tokens[i] != ",") {
                        SelectItem item;
                        std::string col = to_lower(tokens[i]);
                        if (col == "count" || col == "max" || col == "min" || col == "avg") {
                            if (i + 1 < tokens.size() && tokens[i+1] == "(") {
                                if (col == "count") item.aggr = AggrFunc::COUNT;
                                else if (col == "max") item.aggr = AggrFunc::MAX;
                                else if (col == "min") item.aggr = AggrFunc::MIN;
                                else if (col == "avg") item.aggr = AggrFunc::AVG;
                                i += 2; // skip func and '('
                                item.column = to_lower(tokens[i++]);
                                if (i >= tokens.size() || tokens[i++] != ")") throw std::runtime_error("Expected ')'");
                            } else {
                                item.column = col;
                                i++;
                            }
                        } else {
                            item.column = col;
                            i++;
                        }
                        cmd.select_items.push_back(item);
                    } else {
                        i++;
                    }
                }
            }
            if (i >= tokens.size() || to_lower(tokens[i++]) != "from") throw std::runtime_error("Expected FROM keyword");
            cmd.table_name = to_lower(tokens[i++]);
            
            parse_where(tokens, i, cmd);
            
            if (i < tokens.size() && to_lower(tokens[i]) == "group") {
                i++;
                if (i >= tokens.size() || to_lower(tokens[i++]) != "by") throw std::runtime_error("Expected BY after GROUP");
                if (i >= tokens.size()) throw std::runtime_error("Expected column after GROUP BY");
                cmd.group_by_column = to_lower(tokens[i++]);
            }
        } else if (first == "alter") {
            if (tokens.size() < 4) throw std::runtime_error("Syntax error in ALTER statement");
            if (to_lower(tokens[1]) != "table") throw std::runtime_error("Expected TABLE after ALTER");
            cmd.type = CommandType::ALTER_TABLE;
            cmd.table_name = to_lower(tokens[2]);
            std::string action = to_lower(tokens[3]);
            if (action == "add") {
                if (tokens.size() < 6) throw std::runtime_error("Syntax error in ALTER TABLE ADD");
                cmd.alter_type = AlterType::ADD_COLUMN;
                cmd.alter_col_name = to_lower(tokens[4]);
                std::string type_str = to_lower(tokens[5]);
                if (type_str == "int") cmd.alter_col_type = DataType::INT;
                else if (type_str == "string") cmd.alter_col_type = DataType::STRING;
                else if (type_str == "float") cmd.alter_col_type = DataType::FLOAT;
                else throw std::runtime_error("Unsupported data type: " + type_str);
            } else if (action == "drop") {
                if (tokens.size() < 6 || to_lower(tokens[4]) != "column") throw std::runtime_error("Syntax error in ALTER TABLE DROP");
                cmd.alter_type = AlterType::DROP_COLUMN;
                cmd.alter_col_name = to_lower(tokens[5]);
            } else if (action == "rename") {
                if (to_lower(tokens[4]) == "to") {
                    if (tokens.size() < 6) throw std::runtime_error("Syntax error in ALTER TABLE RENAME TO");
                    cmd.alter_type = AlterType::RENAME_TABLE;
                    cmd.alter_new_name = to_lower(tokens[5]);
                } else {
                    if (tokens.size() < 7 || to_lower(tokens[5]) != "to") throw std::runtime_error("Syntax error in ALTER TABLE RENAME COLUMN");
                    cmd.alter_type = AlterType::RENAME_COLUMN;
                    cmd.alter_col_name = to_lower(tokens[4]);
                    cmd.alter_new_name = to_lower(tokens[6]);
                }
            } else {
                throw std::runtime_error("Unknown ALTER action: " + action);
            }
        } else {
            throw std::runtime_error("Unknown statement: " + first);
        }

        return cmd;
    }
};

} // namespace dbms
