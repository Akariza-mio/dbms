#pragma once
#include "core/column.hpp"
#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace minidb{
class Table{
public:
    Table(std::string name, std::vector<Column> columns);

    [[nodiscard]] const std::string& get_name() const noexcept;
    [[nodiscard]] std::size_t get_column_cnt() const noexcept;
    [[nodiscard]] const std::vector<Column>& get_columns() const noexcept;
    [[nodiscard]] const Column& get_column(std::size_t id) const;
    [[nodiscard]] std::optional<std::size_t> get_column_id(const std::string& name) const;
    [[nodiscard]] std::optional<std::size_t> get_pk_id() const;
private:
    std::string name_;
    std::vector<Column> columns_;
};
}