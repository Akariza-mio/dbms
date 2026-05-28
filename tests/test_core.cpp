#include "core/Database.h"
#include <cassert>
#include <iostream>

void test_core_classes() {
    using namespace dbms;

    Database db("test_db");
    assert(db.name == "test_db");
    
    Table t("person");
    t.columns.push_back(Column("id", DataType::INT, true));
    t.columns.push_back(Column("name", DataType::STRING, false));
    
    assert(t.columns.size() == 2);
    assert(t.columns[0].is_primary == true);
    
    Row r1;
    r1.values.push_back(Value(1001));
    r1.values.push_back(Value(std::string("peter")));
    
    t.rows.push_back(r1);
    
    db.tables.push_back(t);
    
    assert(db.tables.size() == 1);
    assert(db.tables[0].rows.size() == 1);
    assert(db.tables[0].rows[0].values[0].int_val == 1001);
    assert(db.tables[0].rows[0].values[1].str_val == "peter");
    
    std::cout << "Core classes tests passed!" << std::endl;
}

int main() {
    test_core_classes();
    return 0;
}
