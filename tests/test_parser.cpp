#include "sql/Parser.h"
#include <cassert>
#include <iostream>

void test_parser() {
    using namespace dbms;

    // CREATE DATABASE
    auto cmd = Parser::parse("create database mydb");
    assert(cmd.type == CommandType::CREATE_DATABASE);
    assert(cmd.db_name == "mydb");

    // CREATE TABLE
    cmd = Parser::parse("create table person (id int primary, name string)");
    assert(cmd.type == CommandType::CREATE_TABLE);
    assert(cmd.table_name == "person");
    assert(cmd.columns.size() == 2);
    assert(cmd.columns[0].name == "id");
    assert(cmd.columns[0].type == DataType::INT);
    assert(cmd.columns[0].is_primary == true);
    assert(cmd.columns[1].name == "name");
    assert(cmd.columns[1].type == DataType::STRING);
    assert(cmd.columns[1].is_primary == false);

    // INSERT
    cmd = Parser::parse("insert person values (1001, \"peter\")");
    assert(cmd.type == CommandType::INSERT);
    assert(cmd.table_name == "person");
    assert(cmd.insert_values.size() == 2);
    assert(cmd.insert_values[0].int_val == 1001);
    assert(cmd.insert_values[1].str_val == "peter");

    // SELECT
    cmd = Parser::parse("select name from person where id = 1001");
    assert(cmd.type == CommandType::SELECT);
    assert(cmd.select_columns[0] == "name");
    assert(cmd.table_name == "person");
    assert(cmd.where_cond.column == "id");
    assert(cmd.where_cond.op == OpType::EQ);
    assert(cmd.where_cond.value.int_val == 1001);
    
    // UPDATE
    cmd = Parser::parse("update person set name = \"mary\" where id = 1001");
    assert(cmd.type == CommandType::UPDATE);
    assert(cmd.table_name == "person");
    assert(cmd.update_column == "name");
    assert(cmd.update_value.str_val == "mary");
    assert(cmd.where_cond.column == "id");
    assert(cmd.where_cond.op == OpType::EQ);
    assert(cmd.where_cond.value.int_val == 1001);

    std::cout << "Parser tests passed!" << std::endl;
}

int main() {
    test_parser();
    return 0;
}
