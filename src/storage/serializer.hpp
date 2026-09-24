#pragma once
#include "core/value.hpp"
#include "core/row.hpp"
#include <cstddef>
#include <vector>

namespace minidb::serialization{
    using byte_vector = std::vector<std::byte>;
    [[nodiscard]] byte_vector serialize_value(const Value& value);
    [[nodiscard]] Value deserialize_value(const byte_vector& bytes);
    [[nodiscard]] byte_vector serialize_row(const Row& row);
    [[nodiscard]] Row deserialize_row(const byte_vector& bytes);
}