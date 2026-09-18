#pragma once
#include "core/value.hpp"
#include <vector>
#include <cstddef>

namespace minidb{
class Row{
public:
    explicit Row(std::vector<Value> values);

    [[nodiscard]] std::size_t get_size() const noexcept;
    [[nodiscard]] const Value& get_value(std::size_t id) const;
    [[nodiscard]] const std::vector<Value>& get_values() const noexcept;
private:
    std::vector<Value> values_;
};
}