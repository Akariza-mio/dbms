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
    
    // ALTER TABLE ADD COLUMN
    res = exec.execute(Parser::parse("alter table person add age int"));
    assert(res == "Column added successfully.");
    
    // Insert after add column
    res = exec.execute(Parser::parse("insert person values (1003, \"tom\", 20)"));
    assert(res == "Insert successful.");
    
    // Select to verify new column
    res = exec.execute(Parser::parse("select age from person where id = 1003"));
    assert(res.find("20") != std::string::npos);

    // ADD FLOAT COLUMN
    res = exec.execute(Parser::parse("alter table person add score float"));
    assert(res == "Column added successfully.");
    res = exec.execute(Parser::parse("insert person values (1004, \"alice\", 25, 95.5)"));
    assert(res == "Insert successful.");
    res = exec.execute(Parser::parse("insert person values (1005, \"bob\", 25, 80.2)"));
    assert(res == "Insert successful.");
    
    // Test MAX, MIN, AVG, COUNT
    res = exec.execute(Parser::parse("select count(id), max(score), min(score), avg(score) from person where age = 25"));
    assert(res.find("2") != std::string::npos);
    assert(res.find("95.500") != std::string::npos);
    assert(res.find("80.200") != std::string::npos);
    assert(res.find("87.850") != std::string::npos);
    
    // Test GROUP BY
    res = exec.execute(Parser::parse("select age, count(id), max(score) from person group by age"));
    assert(res.find("95.500") != std::string::npos);

    // Default values check
    res = exec.execute(Parser::parse("select age from person where id = 1001"));
    assert(res.find("0") != std::string::npos); // Should be default 0

    // ALTER TABLE RENAME COLUMN
    res = exec.execute(Parser::parse("alter table person rename age to years"));
    assert(res == "Column renamed successfully.");

    // ALTER TABLE DROP COLUMN
    res = exec.execute(Parser::parse("alter table person drop column years"));
    assert(res == "Column dropped successfully.");
    
    // Test drop primary key fails
    res = exec.execute(Parser::parse("alter table person drop column id"));
    assert(res.find("Error") != std::string::npos);

    // ALTER TABLE RENAME TABLE
    res = exec.execute(Parser::parse("alter table person rename to human"));
    assert(res == "Table renamed successfully.");
    
    // Verify old table gone
    res = exec.execute(Parser::parse("select * from person"));
    assert(res.find("Error") != std::string::npos);
    
    // Verify new table exists
    res = exec.execute(Parser::parse("select name from human where id = 1003"));
    assert(res.find("tom") != std::string::npos);

    std::cout << "Executor tests passed!" << std::endl;
}

int main() {
    test_executor();
    return 0;
}
