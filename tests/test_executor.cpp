#include "sql/Parser.h"
#include "sql/Executor.h"
#include <iostream>
#include <cassert>

void test_executor() {
    using namespace dbms;
    
    // Clean up
    system("rm -rf data");
    
    Executor exec;
    
    // Create DB
    std::string res = exec.execute(Parser::parse("create database mydb"));
    assert(res == "Database created successfully.");
    
    res = exec.execute(Parser::parse("use mydb"));
    assert(res == "Database changed.");
    
    // Create Table
    res = exec.execute(Parser::parse("create table person (id int primary, name string)"));
    assert(res == "Table created successfully.");
    
    // Insert
    res = exec.execute(Parser::parse("insert person values (1001, \"peter\")"));
    assert(res == "Insert successful.");
    
    res = exec.execute(Parser::parse("insert person values (1002, \"mary\")"));
    assert(res == "Insert successful.");
    
    // Duplicate primary key
    res = exec.execute(Parser::parse("insert person values (1001, \"john\")"));
    assert(res.find("Error") != std::string::npos);
    
    // Select
    res = exec.execute(Parser::parse("select name from person where id = 1001"));
    assert(res.find("peter") != std::string::npos);
    
    // Update
    res = exec.execute(Parser::parse("update person set name = \"parker\" where id = 1001"));
    assert(res == "Updated 1 rows.");
    
    res = exec.execute(Parser::parse("select name from person where id = 1001"));
    assert(res.find("parker") != std::string::npos);
    
    // Delete
    res = exec.execute(Parser::parse("delete person where id = 1002"));
    assert(res == "Deleted 1 rows.");
    
    res = exec.execute(Parser::parse("select name from person where id = 1002"));
    assert(res.find("0 rows") != std::string::npos);
    
    std::cout << "Executor tests passed!" << std::endl;
}

int main() {
    test_executor();
    return 0;
}
