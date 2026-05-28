#pragma once
#include <string>
#include "common/Vector.h"
#include "core/Types.h"
#include "core/Column.h"

namespace dbms {

enum class CommandType {
    CREATE_DATABASE,
    DROP_DATABASE,
    USE_DATABASE,
    SHOW_DATABASES,
    CREATE_TABLE,
    DROP_TABLE,
    INSERT,
    DELETE,
    UPDATE,
    SELECT,
    EXIT,
    UNKNOWN
};

enum class OpType { EQ, LT, GT, NONE };

struct Condition {
    std::string column;
    OpType op;
    Value value;
    Condition() : op(OpType::NONE) {}
};

struct SQLCommand {
    CommandType type = CommandType::UNKNOWN;
    std::string db_name; 
    std::string table_name; 
    
    Vector<Column> columns;
    Vector<Value> insert_values;
    
    std::string update_column;
    Value update_value;
    
    Vector<std::string> select_columns; 
    
    Condition where_cond;
};

} // namespace dbms
