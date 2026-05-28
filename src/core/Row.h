#pragma once

#include "common/Vector.h"
#include "Types.h"

namespace dbms {

class Row {
public:
    Vector<Value> values;

    Row() = default;
    
    Row(const Row& other) : values(other.values) {}
    Row(Row&& other) noexcept : values(std::move(other.values)) {}
    
    Row& operator=(const Row& other) {
        if (this != &other) values = other.values;
        return *this;
    }
    Row& operator=(Row&& other) noexcept {
        if (this != &other) values = std::move(other.values);
        return *this;
    }
};

} // namespace dbms
