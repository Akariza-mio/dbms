#pragma once
#include "Database.h"
#include <fstream>
#include <sstream>

namespace dbms {

class Catalog {
private:
    Vector<Database> databases_;
    std::string filepath_;

public:
    Catalog(const std::string& filepath = "data/catalog.meta") : filepath_(filepath) {
        load();
    }

    void load() {
        databases_.clear();
        std::ifstream file(filepath_);
        if (!file.is_open()) return;
        
        std::string line;
        Database* current_db = nullptr;
        Table* current_table = nullptr;
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string type;
            ss >> type;
            if (type == "DB") {
                std::string name;
                ss >> name;
                databases_.push_back(Database(name));
                current_db = &databases_[databases_.size() - 1];
            } else if (type == "TABLE" && current_db) {
                std::string name;
                ss >> name;
                current_db->tables.push_back(Table(name));
                current_table = &current_db->tables[current_db->tables.size() - 1];
            } else if (type == "COL" && current_table) {
                std::string name;
                int dtype;
                int is_primary;
                ss >> name >> dtype >> is_primary;
                current_table->columns.push_back(Column(name, static_cast<DataType>(dtype), is_primary != 0));
            }
        }
    }

    void save() {
        std::ofstream file(filepath_, std::ios::trunc);
        for (size_t i = 0; i < databases_.size(); ++i) {
            file << "DB " << databases_[i].name << "\n";
            for (size_t j = 0; j < databases_[i].tables.size(); ++j) {
                file << "TABLE " << databases_[i].tables[j].name << "\n";
                for (size_t k = 0; k < databases_[i].tables[j].columns.size(); ++k) {
                    file << "COL " << databases_[i].tables[j].columns[k].name << " " 
                         << static_cast<int>(databases_[i].tables[j].columns[k].type) << " "
                         << (databases_[i].tables[j].columns[k].is_primary ? 1 : 0) << "\n";
                }
            }
        }
    }

    const Vector<Database>& get_databases() const {
        return databases_;
    }

    Database* get_database(const std::string& name) {
        for (size_t i = 0; i < databases_.size(); ++i) {
            if (databases_[i].name == name) return &databases_[i];
        }
        return nullptr;
    }

    bool create_database(const std::string& name) {
        if (get_database(name)) return false;
        databases_.push_back(Database(name));
        save();
        return true;
    }

    bool drop_database(const std::string& name) {
        for (size_t i = 0; i < databases_.size(); ++i) {
            if (databases_[i].name == name) {
                databases_.erase(i);
                save();
                return true;
            }
        }
        return false;
    }
};

} // namespace dbms
