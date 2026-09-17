#include "core/column.hpp"
#include <string>
#include <stdexcept>
#include <utility>

namespace minidb{
    //constructor
    Column::Column(std::string name,DataType type,bool primary_key)
        : name (std::move(name)),type(type),primary_key(primary_key) {
        //check for valid column name
        if(this->name.empty()){
            throw std::invalid_argument("Column name cannot be empty");
        }
        if(this->name[0] < 'a' || this->name[0] > 'z'){
            throw std::invalid_argument("Column name must start with a lowercase letter");
        }
        for(char c : this->name){
            if(( c < 'a' || c > 'z' ) && ( c < '0' || c > '9')){
                throw std::invalid_argument("Column name must contain only lowercase letters or digits");
            }
        }
        if(primary_key && type == DataType::FLOAT){
            throw std::invalid_argument("Primary key cannot be of type FLOAT");
        }
    }
    //getters
    const std::string& Column::get_name() const noexcept{
        return this->name;
    }
    DataType Column::get_type() const noexcept{
        return this->type;
    }
    bool Column::is_primary_key() const noexcept{
        return this->primary_key;
    }
}