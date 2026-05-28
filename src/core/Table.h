#pragma once

#include <string>
#include "common/Vector.h"
#include "Column.h"
#include "Row.h"

namespace dbms {

class Table {
public:
    std::string name;
    Vector<Column> columns;
    Vector<Row> rows;

    Table() = default;
    explicit Table(const std::string& n) : name(n) {}
};

} // namespace dbms
