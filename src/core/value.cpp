#include "core/value.hpp"
#include <stdexcept>
#include <utility>

namespace minidb{
//constructor
Value::Value(std::int32_t value) : data_(value) {}
Value::Value(std::string value) : data_(std::move(value)) {
    if(std::get<std::string>(data_).size() > MAX_STRING_LENGTH){
        throw std::invalid_argument("String value exceeds maximum length of 256 bytes");
    }
}
Value::Value(double value) : data_(value) {}
// get the type
DataType Value::get_type() const noexcept{
    if(std::holds_alternative<std::int32_t>(data_)){
        return DataType::INT;
    }
    if(std::holds_alternative<std::string>(data_)){
        return DataType::STRING;
    }
    return DataType::FLOAT;
}
//get the value
std::int32_t Value::as_int() const{
    if(!std::holds_alternative<std::int32_t>(data_)){
        throw std::logic_error("Value is not INT");
    }
    return std::get<std::int32_t>(data_);
}
const std::string& Value::as_string() const{
    if(!std::holds_alternative<std::string>(data_)){
        throw std::logic_error("Value is not STRING");
    }
    return std::get<std::string>(data_);
}
double Value::as_float() const{
    if(!std::holds_alternative<double>(data_)){
        throw std::logic_error("Value is not FLOAT");
    }
    return std::get<double>(data_);
}
//for showing in CLI
std::string Value::to_string() const{
    switch(get_type()){
        case DataType::INT:
            return std::to_string(as_int());
        case DataType::FLOAT:
            return std::to_string(as_float());
        case DataType::STRING:
            return as_string();
    }
    throw std::logic_error("Unknown data type");
}
//comparing
bool Value::operator==(const Value& other) const{
    if(get_type() != other.get_type()){
        throw std::logic_error("Cannot compare values of different types");
    } 
    DataType type = get_type();
    switch(type){
        case DataType::INT:
            return as_int() == other.as_int();
        case DataType::FLOAT:
            return as_float() == other.as_float();
        case DataType::STRING:
            return as_string() == other.as_string();
        default:
            throw std::logic_error("Unknown data type");
    }
    return false;
}
bool Value::operator<(const Value& other) const{
    if(get_type() != other.get_type()){
        throw std::logic_error("Cannot compare values of different types");
    } 
    DataType type = get_type();
    switch(type){
        case DataType::INT:
            return as_int() < other.as_int();
        case DataType::FLOAT:
            return as_float() < other.as_float();
        case DataType::STRING:
            return as_string() < other.as_string();
        default:
            throw std::logic_error("Unknown data type");
    }
}
bool Value::operator>(const Value& other)const{
    return other < *this;
}
}


    
