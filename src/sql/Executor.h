#pragma once
#include "AST.h"
#include "core/Catalog.h"
#include "storage/TableFile.h"
#include "storage/BPlusTree.h"
#include <string>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <sstream>
#include <sys/stat.h>

namespace dbms {

std::string row_to_string(const Row& row) {
    std::string res;
    for (size_t i = 0; i < row.values.size(); ++i) {
        if (row.values[i].type == DataType::INT) {
            res += std::to_string(row.values[i].int_val) + "\t";
        } else {
            res += row.values[i].str_val + "\t";
        }
    }
    return res;
}

class Executor {
private:
    Catalog catalog_;
    std::string current_db_;

    std::string get_table_path(const std::string& table_name) {
        return "data/" + current_db_ + "/" + table_name + ".dat";
    }
    
    std::string get_index_path(const std::string& table_name) {
        return "data/" + current_db_ + "/" + table_name + ".idx";
    }

    int get_col_index(Table* table, const std::string& col_name) {
        for (size_t i = 0; i < table->columns.size(); ++i) {
            if (table->columns[i].name == col_name) return i;
        }
        return -1;
    }
    
    int get_primary_col_index(Table* table) {
        for (size_t i = 0; i < table->columns.size(); ++i) {
            if (table->columns[i].is_primary) return i;
        }
        return -1;
    }

public:
    Executor() {
        mkdir("data", 0777); 
    }

    std::string execute(const SQLCommand& cmd) {
        try {
            switch (cmd.type) {
                case CommandType::CREATE_DATABASE:
                    if (catalog_.create_database(cmd.db_name)) {
                        mkdir(("data/" + cmd.db_name).c_str(), 0777);
                        return "Database created successfully.";
                    }
                    return "Error: Database already exists.";
                    
                case CommandType::DROP_DATABASE:
                    if (catalog_.drop_database(cmd.db_name)) {
                        std::string cmd_rm = "rm -rf data/" + cmd.db_name;
                        system(cmd_rm.c_str());
                        if (current_db_ == cmd.db_name) current_db_ = "";
                        return "Database dropped successfully.";
                    }
                    return "Error: Database does not exist.";
                    
                case CommandType::USE_DATABASE:
                    if (catalog_.get_database(cmd.db_name)) {
                        current_db_ = cmd.db_name;
                        return "Database changed.";
                    }
                    return "Error: Database does not exist.";
                    
                case CommandType::SHOW_DATABASES: {
                    std::string res = "Databases:\n--------------------\n";
                    const auto& dbs = catalog_.get_databases();
                    for (size_t i = 0; i < dbs.size(); ++i) {
                        res += dbs[i].name + "\n";
                    }
                    return res + "(" + std::to_string(dbs.size()) + " rows)";
                }
                    
                case CommandType::CREATE_TABLE: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) {
                            return "Error: Table already exists.";
                        }
                    }
                    
                    Table t(cmd.table_name);
                    t.columns = cmd.columns;
                    db->tables.push_back(t);
                    catalog_.save();
                    
                    TableFile tf(get_table_path(cmd.table_name));
                    if (get_primary_col_index(&t) != -1) {
                        Pager p(get_index_path(cmd.table_name));
                        BPlusTree bpt(&p);
                    }
                    return "Table created successfully.";
                }
                    
                case CommandType::DROP_TABLE: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    bool found = false;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) {
                            db->tables.erase(i);
                            found = true;
                            break;
                        }
                    }
                    if (!found) return "Error: Table does not exist.";
                    catalog_.save();
                    std::remove(get_table_path(cmd.table_name).c_str());
                    std::remove(get_index_path(cmd.table_name).c_str());
                    return "Table dropped successfully.";
                }
                    
                case CommandType::INSERT: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    Table* t = nullptr;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) { t = &db->tables[i]; break; }
                    }
                    if (!t) return "Error: Table does not exist.";
                    if (cmd.insert_values.size() != t->columns.size()) {
                        return "Error: Column count doesn't match value count.";
                    }
                    
                    Row r;
                    r.values = cmd.insert_values;
                    
                    int pk_idx = get_primary_col_index(t);
                    TableFile tf(get_table_path(cmd.table_name));
                    
                    if (pk_idx != -1) {
                        Pager p(get_index_path(cmd.table_name));
                        BPlusTree bpt(&p);
                        uint32_t dummy;
                        if (bpt.search(r.values[pk_idx], dummy)) {
                            return "Error: Duplicate entry for primary key.";
                        }
                        uint32_t offset = tf.insert_row(r);
                        bpt.insert(r.values[pk_idx], offset);
                    } else {
                        tf.insert_row(r);
                    }
                    return "Insert successful.";
                }
                
                case CommandType::SELECT: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    Table* t = nullptr;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) { t = &db->tables[i]; break; }
                    }
                    if (!t) return "Error: Table does not exist.";
                    
                    TableFile tf(get_table_path(cmd.table_name));
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    tf.scan_all_rows(all_rows, all_offs);
                    
                    int where_col_idx = -1;
                    if (cmd.where_cond.op != OpType::NONE) {
                        where_col_idx = get_col_index(t, cmd.where_cond.column);
                        if (where_col_idx == -1) return "Error: Unknown column in where clause.";
                    }
                    
                    std::string res;
                    for (size_t i = 0; i < t->columns.size(); ++i) {
                        res += t->columns[i].name + "\t";
                    }
                    res += "\n--------------------\n";
                    
                    int count = 0;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        bool match = true;
                        if (where_col_idx != -1) {
                            if (cmd.where_cond.op == OpType::EQ) match = all_rows[i].values[where_col_idx] == cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::LT) match = all_rows[i].values[where_col_idx] < cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::GT) match = all_rows[i].values[where_col_idx] > cmd.where_cond.value;
                        }
                        if (match) {
                            res += row_to_string(all_rows[i]) + "\n";
                            count++;
                        }
                    }
                    res += "(" + std::to_string(count) + " rows)";
                    return res;
                }
                
                case CommandType::DELETE: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    Table* t = nullptr;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) { t = &db->tables[i]; break; }
                    }
                    if (!t) return "Error: Table does not exist.";
                    
                    int pk_idx = get_primary_col_index(t);
                    TableFile tf(get_table_path(cmd.table_name));
                    
                    std::unique_ptr<Pager> p;
                    std::unique_ptr<BPlusTree> bpt;
                    if (pk_idx != -1) {
                        p = std::make_unique<Pager>(get_index_path(cmd.table_name));
                        bpt = std::make_unique<BPlusTree>(p.get());
                    }
                    
                    int where_col_idx = -1;
                    if (cmd.where_cond.op != OpType::NONE) {
                        where_col_idx = get_col_index(t, cmd.where_cond.column);
                        if (where_col_idx == -1) return "Error: Unknown column in where clause.";
                    }
                    
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    tf.scan_all_rows(all_rows, all_offs);
                    
                    int count = 0;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        bool match = true;
                        if (where_col_idx != -1) {
                            if (cmd.where_cond.op == OpType::EQ) match = all_rows[i].values[where_col_idx] == cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::LT) match = all_rows[i].values[where_col_idx] < cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::GT) match = all_rows[i].values[where_col_idx] > cmd.where_cond.value;
                        }
                        if (match) {
                            tf.delete_row(all_offs[i]);
                            if (bpt) {
                                bpt->remove(all_rows[i].values[pk_idx]);
                            }
                            count++;
                        }
                    }
                    return "Deleted " + std::to_string(count) + " rows.";
                }

                case CommandType::UPDATE: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    Table* t = nullptr;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) { t = &db->tables[i]; break; }
                    }
                    if (!t) return "Error: Table does not exist.";
                    
                    int pk_idx = get_primary_col_index(t);
                    TableFile tf(get_table_path(cmd.table_name));
                    
                    std::unique_ptr<Pager> p;
                    std::unique_ptr<BPlusTree> bpt;
                    if (pk_idx != -1) {
                        p = std::make_unique<Pager>(get_index_path(cmd.table_name));
                        bpt = std::make_unique<BPlusTree>(p.get());
                    }
                    
                    int set_col_idx = get_col_index(t, cmd.update_column);
                    if (set_col_idx == -1) return "Error: Unknown column in SET clause.";
                    
                    int where_col_idx = -1;
                    if (cmd.where_cond.op != OpType::NONE) {
                        where_col_idx = get_col_index(t, cmd.where_cond.column);
                        if (where_col_idx == -1) return "Error: Unknown column in where clause.";
                    }
                    
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    tf.scan_all_rows(all_rows, all_offs);
                    
                    int count = 0;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        bool match = true;
                        if (where_col_idx != -1) {
                            if (cmd.where_cond.op == OpType::EQ) match = all_rows[i].values[where_col_idx] == cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::LT) match = all_rows[i].values[where_col_idx] < cmd.where_cond.value;
                            else if (cmd.where_cond.op == OpType::GT) match = all_rows[i].values[where_col_idx] > cmd.where_cond.value;
                        }
                        if (match) {
                            tf.delete_row(all_offs[i]);
                            if (bpt) bpt->remove(all_rows[i].values[pk_idx]);
                            
                            all_rows[i].values[set_col_idx] = cmd.update_value;
                            
                            uint32_t new_off = tf.insert_row(all_rows[i]);
                            if (bpt) bpt->insert(all_rows[i].values[pk_idx], new_off);
                            
                            count++;
                        }
                    }
                    return "Updated " + std::to_string(count) + " rows.";
                }

                default:
                    return "Command not implemented.";
            }
        } catch (const std::exception& e) {
            return std::string("Execution Error: ") + e.what();
        }
    }
};

} // namespace dbms
