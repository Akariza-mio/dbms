#pragma once

#include <string>
#include "Types.h"

namespace dbms {

class Column {
public:
    std::string name;
    DataType type;
    bool is_primary;

    Column() = default;
    Column(const std::string& n, DataType t, bool p = false)
        : name(n), type(t), is_primary(p) {}
};

} // namespace dbms
