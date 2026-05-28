#pragma once

#include <string>
#include "common/Vector.h"
#include "Table.h"

namespace dbms {

class Database {
public:
    std::string name;
    Vector<Table> tables;

    Database() = default;
    explicit Database(const std::string& n) : name(n) {}
};

} // namespace dbms
