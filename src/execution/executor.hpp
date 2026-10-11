#pragma once
#include "core/catalog.hpp"
#include "core/row.hpp"
#include "sql/ast.hpp"
#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace minidb{
class ExecutionError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
struct ExecutionResult {
    std::string message;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::size_t affected_rows = 0;
};
class Executor {
public:
    explicit Executor(const std::filesystem::path& data_directory);
    [[nodiscard]] ExecutionResult execute(const sql::SQLCommand& command);
    [[nodiscard]] const std::optional<std::string>& get_now_database() const noexcept;
private:
    std::filesystem::path data_directory_;
    std::filesystem::path catalog_path_;
    Catalog catalog_;
    std::optional<std::string> now_database_;
    [[nodiscard]] ExecutionResult execute_create_database(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_drop_database(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_use_database(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_show_databases(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_create_table(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_drop_table(const sql::SQLCommand& command);
    [[nodiscard]] ExecutionResult execute_show_tables(const sql::SQLCommand& command);
    //check if the database is selected
    [[nodiscard]] const std::string& require_now_database() const;
    [[nodiscard]] std::filesystem::path database_path(const std::string& db_name) const;
    [[nodiscard]] std::filesystem::path table_path(const std::string& db_name,const std::string& table_name) const;
    [[nodiscard]] std::filesystem::path table_index_path(const std::string& db_name,const std::string& table_name) const;
    void save_catalog() const;
    //insert
    [[nodiscard]] ExecutionResult execute_insert(const sql::SQLCommand& command);
    [[nodiscard]] std::vector<Value> normalize_insert_values(const Table& table,const std::vector<Value>& input) const;
    //select 
    struct parsedCondition{
        std::size_t column_id;
        sql::ComparisonOperator comparison;
        Value value;
    };
    [[nodiscard]] std::vector<parsedCondition> parse_conditions(const Table& table,const sql::SQLCommand& command) const;
    //check if the row is matched with the conditions
    [[nodiscard]] bool row_matches_conditions(
        const Row& row,
        const std::vector<parsedCondition>& conditions,
        const std::vector<sql::LogicalOperator>& logical_operators
    ) const;
    //get column id of the column being selected
    [[nodiscard]] std::vector<std::size_t> parse_projection_columns(
        const Table& table,
        const sql::SQLCommand& command
    ) const;
    [[nodiscard]] ExecutionResult execute_select(const sql::SQLCommand& command)const;
};
}
