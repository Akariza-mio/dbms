#pragma once
#include "core/data_type.hpp"
#include <cstdint>
#include <string>
#include <variant>

namespace minidb{
class Value{
public:
    //def of max string length
    static constexpr std::size_t MAX_STRING_LENGTH = 256;

    explicit Value(std::int32_t value);
    explicit Value(std::string value);
    explicit Value(double value);

    [[nodiscard]] DataType get_type() const noexcept;
    [[nodiscard]] std::int32_t as_int() const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] double as_float() const; 
    [[nodiscard]] std::string to_string() const;

    bool operator==(const Value& other) const;
    bool operator<(const Value& other) const;
    bool operator>(const Value& other) const;
private:
    std::variant<std::int32_t, std::string, double> data_;
};
}