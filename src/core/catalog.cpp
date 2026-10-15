#include "core/catalog.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include <fstream>
#include <system_error>
#include <charconv>
#include <string_view>

namespace {
    //only used in this file
    std::string data_type_to_text(minidb::DataType type){
        switch(type){
            case minidb::DataType::INT:
                return "INT";
            case minidb::DataType::FLOAT:
                return "FLOAT";
            case minidb::DataType::STRING:
                return "STRING";
            default:
                throw std::runtime_error("Unknown data type");
        }
    }
    //expect a specific token from input stream, throw error if not found
    void expect_token(std::istream& input,std::string_view expect){
        std::string token;
        if(!(input >> token)){
            throw std::runtime_error("unexpected end of file");
        }
        if(token != expect){
            throw std::runtime_error("unexpected token: " + token + ", expected: " + std::string(expect));
        }
    }
    minidb::DataType text_to_data_type(std::string_view text){
        if(text == "INT"){
            return minidb::DataType::INT;
        }else if(text == "FLOAT"){
            return minidb::DataType::FLOAT;
        }else if(text == "STRING"){
            return minidb::DataType::STRING;
        }
        throw std::runtime_error("Unknown data type: " + std::string(text));
    }
    //read a size_t from input stream
    //description helps to identify the context of the read operation in error messages
    std::size_t read_cnt(std::istream& input,std::string_view description){
        std::string token;
        if(!(input >> token)){
            if(input.bad()){
                throw std::runtime_error("I/O error while reading " + std::string(description));
            }
            throw std::runtime_error("unexpected end of file while reading " + std::string(description));
        }
        std::size_t value = 0;
        const char* first = token.data();
        const char* last = token.data() + token.size();
        const auto [end, error] = std::from_chars(first,last,value);
        if(error == std::errc::invalid_argument){
            throw std::runtime_error("invalid number while reading " + std::string(description) + ": " + token);
        }
        if(error == std::errc::result_out_of_range){
            throw std::runtime_error("number out of range while reading " + std::string(description) + ": " + token);
        }
        if(end != last){
            throw std::runtime_error("Invalid " + std::string(description) +
            ": '" + token +
            "' contains extra characters");
        }
        return value;
    }
}
namespace minidb{
    //getters
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
    //mutators
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
    //file management -- catalog.meta
    void Catalog::save_to_file(const std::filesystem::path& path)const{
        std::filesystem::path temp_path = path;
        temp_path += ".tmp";
        if(path.has_parent_path()){
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream ofs(temp_path,std::ios::out | std::ios::trunc);
        if(!ofs.is_open()){
            throw std::runtime_error("Failed to open file for writing");
        }
        ofs << "MINIDBMS_CATALOG 1\n";
        ofs << "DATABASE_COUNT " << databases_.size() << "\n";
        for(const Database& db : databases_){
            ofs << "DATABASE " << db.get_name() << " " << db.get_table_cnt() << "\n";
            for(const Table& table : db.get_tables()){
                ofs << "TABLE " << table.get_name() << " " << table.get_column_cnt() << "\n";
                for(const Column& column : table.get_columns()){
                    ofs << "COLUMN " << column.get_name() << " " << data_type_to_text(column.get_type()) << " ";
                    ofs << (column.is_primary_key() ? "1\n" : "0\n");
                }
                ofs << "END_TABLE\n";
            }
            ofs << "END_DATABASE\n";
        }
        ofs << "END_CATALOG\n";
        ofs.flush();
        ofs.close();
        if(ofs.fail()){
            std::error_code ignored;
            std::filesystem::remove(temp_path, ignored);
            throw std::runtime_error("Failed to write to file");
        }
        std::error_code error;
        std::filesystem::rename(temp_path, path, error);
        if(error){
            std::error_code ignored;
            std::filesystem::remove(temp_path, ignored);
            throw std::runtime_error("Failed to rename temporary file:" + error.message());
        }
    }
    Catalog Catalog::load_from_file(const std::filesystem::path& path){
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if(error){
            throw std::runtime_error("Failed to check if file exists: " + error.message());
        }
        if(!exists){
            return Catalog{};
        }
        std::ifstream ifs(path);
        if(!ifs.is_open()){
            throw std::runtime_error("Failed to open file for reading");
        }
        Catalog res;
        expect_token(ifs, "MINIDBMS_CATALOG");
        expect_token(ifs, "1");
        expect_token(ifs, "DATABASE_COUNT");
        std::size_t db_cnt = read_cnt(ifs, "database count");
        for(std::size_t i = 0 ; i < db_cnt ; ++i){
            expect_token(ifs,"DATABASE");
            std::string db_name;
            if(!(ifs >> db_name)){
                throw std::runtime_error("unexpected end of file while reading database name");
            }
            std::size_t table_cnt = read_cnt(ifs, "table count");
            res.create_database(db_name);
            for(std::size_t j = 0; j < table_cnt; ++j){
                expect_token(ifs, "TABLE");
                std::string table_name;
                if(!(ifs >> table_name)){
                    throw std::runtime_error("unexpected end of file while reading table name");
                }
                std::size_t column_cnt = read_cnt(ifs, "column count");
                std::vector<Column> columns;
                for(std::size_t k = 0; k < column_cnt; ++k){
                    expect_token(ifs,"COLUMN");
                    std::string column_name;
                    if(!(ifs >> column_name)){
                        throw std::runtime_error("unexpected end of file while reading column name");
                    }
                    std::string column_type;
                    if(!(ifs >> column_type)){
                        throw std::runtime_error("unexpected end of file while reading column type");
                    }
                    std::string pk_text;
                    if(!(ifs >> pk_text)){
                        throw std::runtime_error("unexpected end of file while reading primary key flag");
                    }
                    bool pk_flag = false;
                    if(pk_text == "1"){
                        pk_flag = true;
                    }else if(pk_text == "0"){
                        pk_flag = false;
                    }else{
                        throw std::runtime_error("primary key must be 0 or 1");
                    }
                    columns.emplace_back(std::move(column_name),text_to_data_type(column_type),pk_flag);
                }
                expect_token(ifs,"END_TABLE");
                Table table{std::move(table_name),std::move(columns)};
                res.create_table(db_name,std::move(table));
            }
            expect_token(ifs,"END_DATABASE");
        }
        expect_token(ifs,"END_CATALOG");
        std::string extra;
        if(ifs >> extra){
            throw std::runtime_error("unexpected extra data after END_CATALOG: " + extra);
        }
        if(ifs.bad()){
            throw std::runtime_error("I/O error while reading catalog file");
        }
        return res;
    }
    void Catalog::replace_table(const std::string& db_name,const std::string& old_name,Table replacement){
        Database& db = get_database(db_name);
        db.replace_table(old_name,std::move(replacement));
    }
}