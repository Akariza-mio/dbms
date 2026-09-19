#pragma once

#include "core/database.hpp"
#include <vector>
#include <string>
#include <optional>
#include <cstddef>

namespace minidb{
class Catalog{
public:
    Catalog() = default;

    [[nodiscard]] std::size_t get_database_cnt() const noexcept;
    [[nodiscard]] const std::vector<Database>& get_databases() const noexcept;
    [[nodiscard]] std::optional<std::size_t> get_database_id(const std::string& name) const;
    [[nodiscard]] Database& get_database(const std::string& name);
    [[nodiscard]] const Database& get_database(const std::string& name) const;
    void create_database(std::string name);
    void drop_database(const std::string& name);

    void create_table(const std::string& db_name,Table table);
    void drop_table(const std::string& db_name,const std::string& table_name);
    [[nodiscard]] Table& get_table(const std::string& db_name,const std::string& table_name);
    [[nodiscard]] const Table& get_table(const std::string& db_name,const std::string& table_name) const;
private:
    std::vector<Database> databases_;
};
}