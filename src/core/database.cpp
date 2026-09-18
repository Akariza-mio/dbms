#include "core/database.hpp"
#include <stdexcept>
#include <utility>

namespace minidb{
    //constructor
    Database::Database(std::string name) 
        : name_(std::move(name)){
            if(name_.empty()){
                throw std::invalid_argument("Database name cannot be empty");
            }
            if(name_[0] > 'z' || name_[0] < 'a'){
                throw std::invalid_argument("Database name must start with a lowercase letter");
            }
            for(char c : name_){
                if((c > 'z' || c < 'a') && (c > '9' || c < '0')){
                    throw std::invalid_argument("Database name can only contain lowercase letters, digits");
                }
            }
    }
    //getters
    const std::string& Database::get_name() const noexcept{
        return name_;
    }
    std::size_t Database::get_table_cnt() const noexcept{
        return tables_.size(); 
    }
    const std::vector<Table>& Database::get_tables() const noexcept{
        return tables_;
    }
    std::optional<std::size_t> Database::get_table_id(const std::string& name) const{
        for(std::size_t i = 0; i < tables_.size(); ++i){
            if(tables_[i].get_name() == name){
                return i;
            }
        }
        return std::nullopt;
    }
    Table& Database::get_table(const std::string& name){
        std::optional<std::size_t> table_id = get_table_id(name);
        if(!table_id.has_value()){
            throw std::invalid_argument("Table with the given name does not exist");
        }
        return tables_.at(table_id.value());
    }
    const Table& Database::get_table(const std::string& name) const{
        std::optional<std::size_t> table_id = get_table_id(name);
        if(!table_id.has_value()){
            throw std::invalid_argument("Table with the given name does not exist");
        }
        return tables_.at(table_id.value());
    }
    //manipulators
    void Database::add_table(Table table){
        std::optional<std::size_t> table_id = get_table_id(table.get_name());
        if(table_id.has_value()){
            throw std::invalid_argument("Table with the same name already exists");
        }
        tables_.push_back(std::move(table));
    }
    void Database::drop_table(const std::string& name){
        std::optional<std::size_t> table_id = get_table_id(name);
        if(!table_id.has_value()){
            throw std::invalid_argument("Table with the given name does not exist");
        }
        tables_.erase(tables_.begin() + table_id.value());
    }
}