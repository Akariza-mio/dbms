#pragma once
#include "core/data_type.hpp"
#include <string>

namespace minidb{
class Column{
public:
    Column(std::string name,DataType type,bool primary_key = false);

    [[nodiscard]] const std::string& get_name() const noexcept;
    [[nodiscard]] DataType get_type() const noexcept;
    [[nodiscard]] bool is_primary_key() const noexcept;
private:
    std::string name;
    DataType type;
    bool primary_key;
};
}
