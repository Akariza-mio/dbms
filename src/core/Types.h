#pragma once

#include <string>

namespace dbms {

enum class DataType {
    INT,
    STRING,
    FLOAT
};

class Value {
public:
    DataType type;
    int int_val;
    std::string str_val;

    double float_val;

    Value() : type(DataType::INT), int_val(0), float_val(0.0) {}
    explicit Value(int v) : type(DataType::INT), int_val(v), float_val(0.0) {}
    explicit Value(double v) : type(DataType::FLOAT), int_val(0), float_val(v) {}
    explicit Value(const std::string& v) : type(DataType::STRING), int_val(0), str_val(v), float_val(0.0) {
        if (str_val.length() > 256) {
            str_val = str_val.substr(0, 256);
        }
    }

    bool operator==(const Value& other) const {
        if (type == DataType::STRING || other.type == DataType::STRING) {
            if (type != other.type) return false;
            return str_val == other.str_val;
        }
        double this_val = (type == DataType::INT) ? int_val : float_val;
        double other_val = (other.type == DataType::INT) ? other.int_val : other.float_val;
        return this_val == other_val;
    }

    bool operator<(const Value& other) const {
        if (type == DataType::STRING || other.type == DataType::STRING) {
            if (type != other.type) return type < other.type;
            return str_val < other.str_val;
        }
        double this_val = (type == DataType::INT) ? int_val : float_val;
        double other_val = (other.type == DataType::INT) ? other.int_val : other.float_val;
        return this_val < other_val;
    }
    
    bool operator>(const Value& other) const {
        return other < *this;
    }
};

} // namespace dbms
