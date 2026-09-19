#include "core/catalog.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>

namespace minidb{
    [[nodiscard]] std::size_t Catalog::get_database_cnt() const noexcept{
        return databases_.size();
    }
    [[nodiscard]] const std::vector<Database>& Catalog::get_databases() const noexcept{
        return databases_;
    }
    [[nodiscard]] std::optional<std::size_t> Catalog::get_database_id(const std::string& name) const{
        std::size_t sz = databases_.size();
        for(std::size_t i = 0; i < sz; ++i){
            if(databases_[i].get_name() == name){
                return i;
            }
        }
        return std::nullopt;
    }
    [[nodiscard]] Database& Catalog::get_database(const std::string& name){
        std::optional<std::size_t> id = get_database_id(name);
        if(!id.has_value()){
            throw std::runtime_error("Database not found");
        }
        return databases_.at(id.value());
    }
    [[nodiscard]] const Database& Catalog::get_database(const std::string& name) const{
        std::optional<std::size_t> id = get_database_id(name);
        if(!id.has_value()){
            throw std::runtime_error("Database not found");
        }
        return databases_.at(id.value());
    }
    void Catalog::create_database(std::string name){
        for(const Database& db : databases_){
            if(db.get_name() == name){
                throw std::runtime_error("Database already exists");
            }
        }
        databases_.emplace_back(std::move(name));
    }
    void Catalog::drop_database(const std::string& name){
        std::optional<std::size_t> id = get_database_id(name);
        if(!id.has_value()){
            throw std::runtime_error("Database not found");
        }
        databases_.erase(databases_.begin() + id.value());
    }
    void Catalog::create_table(const std::string& db_name,Table table){
        Database& db = get_database(db_name);
        db.add_table(std::move(table));
    }
    void Catalog::drop_table(const std::string& db_name,const std::string& table_name){
        Database& db = get_database(db_name);
        db.drop_table(table_name);
    }
    [[nodiscard]] Table& Catalog::get_table(const std::string& db_name,const std::string& table_name){
        Database& db = get_database(db_name);
        return db.get_table(table_name);
    }
    [[nodiscard]] const Table& Catalog::get_table(const std::string& db_name,const std::string& table_name) const{
        const Database& db = get_database(db_name);
        return db.get_table(table_name);
    }
}