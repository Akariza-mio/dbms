#include "core/row.hpp"
#include <utility>

namespace minidb{
    //constructor
    Row::Row(std::vector<Value> values) 
        : values_(std::move(values)){}
    //getters
    std::size_t Row::get_size() const noexcept{
        return values_.size();
    }
    const Value& Row::get_value(std::size_t id) const{
        return values_.at(id);
    }
    const std::vector<Value>& Row::get_values() const noexcept{
        return values_;
    }
}