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
#include <cstdio>

namespace dbms {


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

    bool evaluate_where(const Vector<WhereCondition>& conds, const Vector<int>& col_indices, const Row& row) {
        if (conds.empty()) return true;
        bool result = true;
        LogicalOp current_logic = LogicalOp::AND;
        for (size_t i = 0; i < conds.size(); ++i) {
            bool cond_res = false;
            if (conds[i].op == OpType::EQ) cond_res = (row.values[col_indices[i]] == conds[i].value);
            else if (conds[i].op == OpType::LT) cond_res = (row.values[col_indices[i]] < conds[i].value);
            else if (conds[i].op == OpType::GT) cond_res = (row.values[col_indices[i]] > conds[i].value);
            
            if (i == 0) result = cond_res;
            else {
                if (current_logic == LogicalOp::AND) result = result && cond_res;
                else if (current_logic == LogicalOp::OR) result = result || cond_res;
            }
            current_logic = conds[i].next_logic;
        }
        return result;
    }

    bool can_use_index(const Vector<WhereCondition>& conds, const std::string& pk_col_name, Value& out_pk_val) {
        if (pk_col_name.empty() || conds.empty()) return false;
        
        for (size_t i = 0; i < conds.size(); ++i) {
            if (conds[i].next_logic == LogicalOp::OR) return false;
        }
        
        for (size_t i = 0; i < conds.size(); ++i) {
            if (conds[i].column == pk_col_name && conds[i].op == OpType::EQ) {
                out_pk_val = conds[i].value;
                return true;
            }
        }
        return false;
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
                    
                case CommandType::SHOW_TABLES: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    std::string res = "Tables in " + current_db_ + ":\n";
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        res += "- " + db->tables[i].name + " (";
                        for (size_t j = 0; j < db->tables[i].columns.size(); ++j) {
                            res += db->tables[i].columns[j].name + " ";
                            res += (db->tables[i].columns[j].type == DataType::INT ? "int" : "string");
                            if (db->tables[i].columns[j].is_primary) res += " primary";
                            if (j < db->tables[i].columns.size() - 1) res += ", ";
                        }
                        res += ")\n";
                    }
                    return res + "(" + std::to_string(db->tables.size()) + " tables)";
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
                    
                    Vector<int> proj_idx;
                    Vector<size_t> proj_width;
                    if (cmd.select_columns.size() == 1 && cmd.select_columns[0] == "*") {
                        for (size_t i = 0; i < t->columns.size(); ++i) {
                            proj_idx.push_back(i);
                            proj_width.push_back(t->columns[i].name.length() <= 8 ? 8 : 16);
                        }
                    } else {
                        for (size_t i = 0; i < cmd.select_columns.size(); ++i) {
                            int idx = get_col_index(t, cmd.select_columns[i]);
                            if (idx == -1) return "Error: Unknown column in select list.";
                            proj_idx.push_back(idx);
                            proj_width.push_back(t->columns[idx].name.length() <= 8 ? 8 : 16);
                        }
                    }
                    
                    TableFile tf(get_table_path(cmd.table_name));
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    
                    int pk_idx = get_primary_col_index(t);
                    Value pk_val;
                    std::string pk_col_name = pk_idx != -1 ? t->columns[pk_idx].name : "";
                    
                    if (pk_idx != -1 && can_use_index(cmd.where_conds, pk_col_name, pk_val)) {
                        Pager p(get_index_path(cmd.table_name));
                        BPlusTree bpt(&p);
                        uint32_t offset;
                        if (bpt.search(pk_val, offset)) {
                            Row row;
                            if (tf.get_row(offset, row)) {
                                all_rows.push_back(row);
                                all_offs.push_back(offset);
                            }
                        }
                    } else {
                        tf.scan_all_rows(all_rows, all_offs);
                    }
                    
                    Vector<int> where_col_indices;
                    for (size_t i = 0; i < cmd.where_conds.size(); ++i) {
                        int idx = get_col_index(t, cmd.where_conds[i].column);
                        if (idx == -1) return "Error: Unknown column in where clause.";
                        where_col_indices.push_back(idx);
                    }
                    
                    Vector<Row> matched_rows;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        if (evaluate_where(cmd.where_conds, where_col_indices, all_rows[i])) {
                            matched_rows.push_back(all_rows[i]);
                        }
                    }
                    
                    for (size_t j = 0; j < proj_idx.size(); ++j) {
                        size_t max_len = t->columns[proj_idx[j]].name.length();
                        for (size_t i = 0; i < matched_rows.size(); ++i) {
                            std::string val_str;
                            if (matched_rows[i].values[proj_idx[j]].type == DataType::INT) {
                                val_str = std::to_string(matched_rows[i].values[proj_idx[j]].int_val);
                            } else {
                                val_str = matched_rows[i].values[proj_idx[j]].str_val;
                            }
                            if (val_str.length() > max_len) max_len = val_str.length();
                        }
                        proj_width[j] = (max_len > 8) ? 16 : 8;
                    }
                    
                    auto pad_str = [](std::string s, size_t w) {
                        if (s.length() > w) s = s.substr(0, w);
                        if (s.length() < w) s += std::string(w - s.length(), ' ');
                        return s;
                    };
                    
                    std::string res;
                    for (size_t i = 0; i < proj_idx.size(); ++i) {
                        res += pad_str(t->columns[proj_idx[i]].name, proj_width[i]) + " | ";
                    }
                    if (!proj_idx.empty()) {
                        res.pop_back(); res.pop_back(); res.pop_back();
                    }
                    res += "\n";
                    for (size_t i = 0; i < proj_idx.size(); ++i) {
                        res += std::string(proj_width[i], '-') + "-+-";
                    }
                    if (!proj_idx.empty()) {
                        res.pop_back(); res.pop_back(); res.pop_back();
                    }
                    res += "\n";
                    
                    for (size_t i = 0; i < matched_rows.size(); ++i) {
                        for (size_t j = 0; j < proj_idx.size(); ++j) {
                            std::string val_str;
                            if (matched_rows[i].values[proj_idx[j]].type == DataType::INT) {
                                val_str = std::to_string(matched_rows[i].values[proj_idx[j]].int_val);
                            } else {
                                val_str = matched_rows[i].values[proj_idx[j]].str_val;
                            }
                            res += pad_str(val_str, proj_width[j]) + " | ";
                        }
                        if (!proj_idx.empty()) {
                            res.pop_back(); res.pop_back(); res.pop_back();
                        }
                        res += "\n";
                    }
                    res += "(" + std::to_string(matched_rows.size()) + " rows)";
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
                    
                    Vector<int> where_col_indices;
                    for (size_t i = 0; i < cmd.where_conds.size(); ++i) {
                        int idx = get_col_index(t, cmd.where_conds[i].column);
                        if (idx == -1) return "Error: Unknown column in where clause.";
                        where_col_indices.push_back(idx);
                    }
                    
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    Value pk_val;
                    std::string pk_col_name = pk_idx != -1 ? t->columns[pk_idx].name : "";
                    if (bpt && can_use_index(cmd.where_conds, pk_col_name, pk_val)) {
                        uint32_t offset;
                        if (bpt->search(pk_val, offset)) {
                            Row row;
                            if (tf.get_row(offset, row)) {
                                all_rows.push_back(row);
                                all_offs.push_back(offset);
                            }
                        }
                    } else {
                        tf.scan_all_rows(all_rows, all_offs);
                    }
                    
                    int count = 0;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        if (evaluate_where(cmd.where_conds, where_col_indices, all_rows[i])) {
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
                    
                    Vector<int> set_col_indices;
                    for (size_t k = 0; k < cmd.update_columns.size(); ++k) {
                        int idx = get_col_index(t, cmd.update_columns[k]);
                        if (idx == -1) return "Error: Unknown column in SET clause.";
                        set_col_indices.push_back(idx);
                    }
                    
                    Vector<int> where_col_indices;
                    for (size_t i = 0; i < cmd.where_conds.size(); ++i) {
                        int idx = get_col_index(t, cmd.where_conds[i].column);
                        if (idx == -1) return "Error: Unknown column in where clause.";
                        where_col_indices.push_back(idx);
                    }
                    
                    Vector<Row> all_rows;
                    Vector<uint32_t> all_offs;
                    Value pk_val;
                    std::string pk_col_name = pk_idx != -1 ? t->columns[pk_idx].name : "";
                    if (bpt && can_use_index(cmd.where_conds, pk_col_name, pk_val)) {
                        uint32_t offset;
                        if (bpt->search(pk_val, offset)) {
                            Row row;
                            if (tf.get_row(offset, row)) {
                                all_rows.push_back(row);
                                all_offs.push_back(offset);
                            }
                        }
                    } else {
                        tf.scan_all_rows(all_rows, all_offs);
                    }
                    
                    int pk_update_idx = -1;
                    for (size_t k = 0; k < set_col_indices.size(); ++k) {
                        if (set_col_indices[k] == pk_idx) { pk_update_idx = k; break; }
                    }

                    if (pk_idx != -1 && pk_update_idx != -1) {
                        Vector<Value> new_pks;
                        for (size_t i = 0; i < all_rows.size(); ++i) {
                            if (evaluate_where(cmd.where_conds, where_col_indices, all_rows[i])) {
                                if (!(all_rows[i].values[pk_idx] == cmd.update_values[pk_update_idx])) {
                                    uint32_t dummy;
                                    if (bpt->search(cmd.update_values[pk_update_idx], dummy)) {
                                        return "Error: Duplicate entry for primary key.";
                                    }
                                    for (size_t k = 0; k < new_pks.size(); ++k) {
                                        if (new_pks[k] == cmd.update_values[pk_update_idx]) {
                                            return "Error: Duplicate entry for primary key in update set.";
                                        }
                                    }
                                    new_pks.push_back(cmd.update_values[pk_update_idx]);
                                }
                            }
                        }
                    }
                    
                    int count = 0;
                    for (size_t i = 0; i < all_rows.size(); ++i) {
                        if (evaluate_where(cmd.where_conds, where_col_indices, all_rows[i])) {
                            tf.delete_row(all_offs[i]);
                            if (bpt) bpt->remove(all_rows[i].values[pk_idx]);
                            
                            for (size_t k = 0; k < set_col_indices.size(); ++k) {
                                all_rows[i].values[set_col_indices[k]] = cmd.update_values[k];
                            }
                            
                            uint32_t new_off = tf.insert_row(all_rows[i]);
                            if (bpt) bpt->insert(all_rows[i].values[pk_idx], new_off);
                            
                            count++;
                        }
                    }
                    return "Updated " + std::to_string(count) + " rows.";
                }

                case CommandType::ALTER_TABLE: {
                    if (current_db_.empty()) throw std::runtime_error("No database selected");
                    Database* db = catalog_.get_database(current_db_);
                    Table* t = nullptr;
                    for (size_t i = 0; i < db->tables.size(); ++i) {
                        if (db->tables[i].name == cmd.table_name) { t = &db->tables[i]; break; }
                    }
                    if (!t) return "Error: Table does not exist.";

                    if (cmd.alter_type == AlterType::RENAME_TABLE) {
                        for (size_t i = 0; i < db->tables.size(); ++i) {
                            if (db->tables[i].name == cmd.alter_new_name) return "Error: Table name already exists.";
                        }
                        std::rename(get_table_path(cmd.table_name).c_str(), get_table_path(cmd.alter_new_name).c_str());
                        std::rename(get_index_path(cmd.table_name).c_str(), get_index_path(cmd.alter_new_name).c_str());
                        t->name = cmd.alter_new_name;
                        catalog_.save();
                        return "Table renamed successfully.";
                    }
                    else if (cmd.alter_type == AlterType::RENAME_COLUMN) {
                        int col_idx = get_col_index(t, cmd.alter_col_name);
                        if (col_idx == -1) return "Error: Column does not exist.";
                        if (get_col_index(t, cmd.alter_new_name) != -1) return "Error: New column name already exists.";
                        t->columns[col_idx].name = cmd.alter_new_name;
                        catalog_.save();
                        return "Column renamed successfully.";
                    }
                    else if (cmd.alter_type == AlterType::ADD_COLUMN) {
                        if (get_col_index(t, cmd.alter_col_name) != -1) return "Error: Column already exists.";
                        
                        Vector<Row> all_rows;
                        Vector<uint32_t> all_offs;
                        {
                            TableFile tf(get_table_path(cmd.table_name));
                            tf.scan_all_rows(all_rows, all_offs);
                        }
                        
                        t->columns.push_back(Column(cmd.alter_col_name, cmd.alter_col_type, false));
                        catalog_.save();
                        
                        std::remove(get_table_path(cmd.table_name).c_str());
                        TableFile new_tf(get_table_path(cmd.table_name));
                        
                        int pk_idx = get_primary_col_index(t);
                        std::unique_ptr<Pager> p;
                        std::unique_ptr<BPlusTree> bpt;
                        if (pk_idx != -1) {
                            std::remove(get_index_path(cmd.table_name).c_str());
                            p = std::make_unique<Pager>(get_index_path(cmd.table_name));
                            bpt = std::make_unique<BPlusTree>(p.get());
                        }
                        
                        for (size_t i = 0; i < all_rows.size(); ++i) {
                            if (cmd.alter_col_type == DataType::INT) {
                                all_rows[i].values.push_back(Value(0));
                            } else {
                                all_rows[i].values.push_back(Value(""));
                            }
                            uint32_t new_off = new_tf.insert_row(all_rows[i]);
                            if (bpt) bpt->insert(all_rows[i].values[pk_idx], new_off);
                        }
                        
                        return "Column added successfully.";
                    }
                    else if (cmd.alter_type == AlterType::DROP_COLUMN) {
                        int col_idx = get_col_index(t, cmd.alter_col_name);
                        if (col_idx == -1) return "Error: Column does not exist.";
                        if (t->columns[col_idx].is_primary) return "Error: Cannot drop primary key column.";
                        
                        Vector<Row> all_rows;
                        Vector<uint32_t> all_offs;
                        {
                            TableFile tf(get_table_path(cmd.table_name));
                            tf.scan_all_rows(all_rows, all_offs);
                        }
                        
                        t->columns.erase(col_idx);
                        catalog_.save();
                        
                        std::remove(get_table_path(cmd.table_name).c_str());
                        TableFile new_tf(get_table_path(cmd.table_name));
                        
                        int pk_idx = get_primary_col_index(t);
                        std::unique_ptr<Pager> p;
                        std::unique_ptr<BPlusTree> bpt;
                        if (pk_idx != -1) {
                            std::remove(get_index_path(cmd.table_name).c_str());
                            p = std::make_unique<Pager>(get_index_path(cmd.table_name));
                            bpt = std::make_unique<BPlusTree>(p.get());
                        }
                        
                        for (size_t i = 0; i < all_rows.size(); ++i) {
                            all_rows[i].values.erase(col_idx);
                            uint32_t new_off = new_tf.insert_row(all_rows[i]);
                            if (bpt) bpt->insert(all_rows[i].values[pk_idx], new_off);
                        }
                        
                        return "Column dropped successfully.";
                    }
                    break;
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
