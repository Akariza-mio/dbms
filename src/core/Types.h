#pragma once

#include <string>

namespace dbms {

enum class DataType {
    INT,
    STRING
};

class Value {
public:
    DataType type;
    int int_val;
    std::string str_val;

    Value() : type(DataType::INT), int_val(0) {}
    explicit Value(int v) : type(DataType::INT), int_val(v) {}
    explicit Value(const std::string& v) : type(DataType::STRING), int_val(0), str_val(v) {
        if (str_val.length() > 256) {
            str_val = str_val.substr(0, 256);
        }
    }

    bool operator==(const Value& other) const {
        if (type != other.type) return false;
        if (type == DataType::INT) return int_val == other.int_val;
        return str_val == other.str_val;
    }

    bool operator<(const Value& other) const {
        if (type != other.type) return type < other.type;
        if (type == DataType::INT) return int_val < other.int_val;
        return str_val < other.str_val;
    }
    
    bool operator>(const Value& other) const {
        return other < *this;
    }
};

} // namespace dbms
