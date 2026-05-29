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
    SHOW_TABLES,
    CREATE_TABLE,
    DROP_TABLE,
    INSERT,
    DELETE,
    UPDATE,
    SELECT,
    ALTER_TABLE,
    EXIT,
    UNKNOWN
};

enum class OpType { EQ, LT, GT, NONE };

enum class LogicalOp {
    NONE,
    AND,
    OR
};

enum class AlterType {
    ADD_COLUMN,
    DROP_COLUMN,
    RENAME_TABLE,
    RENAME_COLUMN
};

struct WhereCondition {
    std::string column;
    OpType op = OpType::NONE;
    Value value;
    LogicalOp next_logic = LogicalOp::NONE;
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
    
    Vector<WhereCondition> where_conds;

    AlterType alter_type;
    std::string alter_col_name;
    std::string alter_new_name;
    DataType alter_col_type;
};

} // namespace dbms
