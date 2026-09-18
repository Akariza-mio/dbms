#pragma once
#include "core/table.hpp"
#include <string>
#include <vector>
#include <cstddef>
#include <optional>

namespace minidb{
class Database{
public:
    explicit Database(std::string name);

    [[nodiscard]] const std::string& get_name() const noexcept;
    [[nodiscard]] std::size_t get_table_cnt() const noexcept;
    [[nodiscard]] const std::vector<Table>& get_tables() const noexcept;
    [[nodiscard]] std::optional<std::size_t> get_table_id(const std::string& name) const;
    [[nodiscard]] Table& get_table(const std::string& name);
    [[nodiscard]] const Table& get_table(const std::string& name) const;
    void add_table(Table table);
    void drop_table(const std::string& name);
private:
    std::string name_;
    std::vector<Table> tables_;
};
}