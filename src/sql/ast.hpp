#pragma once
#include "core/data_type.hpp"
#include "core/value.hpp"
#include <string>
#include <vector>
#include <optional>

namespace minidb::sql{
    //all commands
    enum class CommandType{
        CREATE_DATABASE,
        DROP_DATABASE,
        USE_DATABASE,
        SHOW_DATABASES,
        CREATE_TABLE,
        DROP_TABLE,
        ALTER_TABLE,
        SHOW_TABLES,
        INSERT,
        DELETE,
        UPDATE,
        SELECT
    };
    //= < >
    enum class ComparisonOperator{
        EQUAL,
        LESS,
        GREATER
    };
    // && ||
    enum class LogicalOperator{
        AND,
        OR
    };
    // count(*)...
    enum class AggregateType{
        NONE,
        COUNT,
        MAX,
        MIN,
        AVG
    };
    enum class AlterType{
        ADD_COLUMN,
        DROP_COLUMN,
        RENAME_COLUMN,
        RENAME_TABLE
    };
    // * ,<column_name>, count(<column_name>)
    enum class SelectType{
        ALL,
        COLUMN,
        AGGREGATE
    };
    //information of a column
    struct ColumnDefinition{
        std::string name;
        DataType type;
        bool is_primary_key = false;
    };
    //a where condition,age > 18
    struct Condition{
        std::string column_name;
        ComparisonOperator comparison;
        Value value;
    };
    //age = 25
    struct Assignment{
        std::string column_name;
        Value value;
    };
    //a select item = count(id) or "name" or *
    struct SelectItem{
        SelectType type;
        std::string column_name;
        AggregateType aggregate = AggregateType::NONE;
    };
    struct SQLCommand{
        CommandType type;
        std::string database_name;
        std::string table_name;
        //create table
        std::vector<ColumnDefinition> columns;
        //insert
        std::vector<Value> values;
        //update set
        std::vector<Assignment> assignments;
        //select
        std::vector<SelectItem> select_items;
        //where 
        std::vector<Condition> conditions;
        std::vector<LogicalOperator> logical_operators;
        //group by
        std::optional<std::string> group_by_column;
        //alter table
        std::optional<AlterType> alter_type;
        std::optional<ColumnDefinition> alter_column;
        std::optional<std::string> target_column_name;
        std::optional<std::string> new_name;
    };
}