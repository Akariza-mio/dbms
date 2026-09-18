#include "core/table.hpp"
#include <stdexcept>
#include <unordered_set>
#include <utility>


namespace minidb{
    //constructor
    Table::Table(std::string name, std::vector<Column> columns)
        : name_(std::move(name)), columns_(std::move(columns)){
            if(name_.empty()){
                throw std::invalid_argument("Table name cannot be empty");
            }
            if(name_[0] > 'z' || name_[0] < 'a'){
                throw std::invalid_argument("Table name must start with a lowercase letter");
            }
            for(char c : name_){
                if((c > 'z' || c < 'a') && (c > '9' || c < '0')){
                    throw std::invalid_argument("Table name can only contain lowercase letters, digits");
                }
            }
            if(columns_.empty()){
                throw std::invalid_argument("Table must have at least one column");
            }
            std::unordered_set<std::string> column_names;
            for(const Column& col : columns_){
                if(column_names.count(col.get_name()) > 0){
                    throw std::invalid_argument("Column names must be unique");
                }
                column_names.insert(col.get_name());
            }
            std::size_t pk_cnt = 0;
            for(const Column& col : columns_){
                if(col.is_primary_key()){
                    ++pk_cnt;
                }
                if(pk_cnt > 1){
                    throw std::invalid_argument("Table can only have one primary key");
                }
            }
        }
    //getters
    const std::string& Table::get_name() const noexcept{
        return name_;
    }
    std::size_t Table::get_column_cnt() const noexcept{
        return columns_.size();
    }
    const std::vector<Column>& Table::get_columns() const noexcept{
        return columns_;
    }
    const Column& Table::get_column(std::size_t id) const{
        return columns_.at(id);
    }
    std::optional<std::size_t> Table::get_column_id(const std::string& name) const{
        std::size_t sz = columns_.size();
        for(std::size_t i = 0; i < sz; ++i){
            if(columns_[i].get_name() == name){
                return i;
            }
        }
        return std::nullopt;
    }
    std::optional<std::size_t> Table::get_pk_id() const{
        std::size_t sz = columns_.size();
        for(std::size_t i = 0; i < sz; ++i){
            if(columns_[i].is_primary_key()){
                return i;
            }
        }
        return std::nullopt;
    }
}