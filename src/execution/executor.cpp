#include "execution/executor.hpp"
#include "storage/b_plus_tree.hpp"
#include "storage/table_file.hpp"
#include <system_error>
#include <utility>

namespace minidb {
    Executor::Executor(const std::filesystem::path& data_directory)
        : data_directory_(data_directory),catalog_path_(data_directory_/"catalog.meta"){
            if(data_directory_.empty()){
                throw ExecutionError("data directory path is empty");
            }
            std::error_code error;
            //create data directory
            std::filesystem::create_directories(data_directory_,error);
            if(error){
                throw ExecutionError("failed to create data directory: " + data_directory_.string() + ": " + error.message());
            }
            //check if data directory is a usable directory
            const bool is_directory = std::filesystem::is_directory(data_directory_,error);
            if(error){
                throw ExecutionError("failed to check if data directory is a directory: " + data_directory_.string() + ": " + error.message());
            }
            if(!is_directory){
                throw ExecutionError("data path is not a directory: " + data_directory_.string());
            }
            //catch exception if failed to load catalog and throw
            try {
                catalog_ = Catalog::load_from_file(catalog_path_);
            } catch (const std::exception& exception){
                throw ExecutionError("failed to load catalog from file: " + catalog_path_.string() + ": " + exception.what());
            }
        }
    const std::optional<std::string>& Executor::get_now_database() const noexcept{
        return now_database_;
    }
    const std::string& Executor::require_now_database() const{
        if(!now_database_.has_value()){
            throw ExecutionError("No database selected");
        }
        return now_database_.value();
    }
    std::filesystem::path Executor::database_path(const std::string& db_name) const{
        return data_directory_ / db_name;
    }
    std::filesystem::path Executor::table_path(const std::string& db_name,const std::string& table_name) const{
        return database_path(db_name) / (table_name + ".dat");
    }
    std::filesystem::path Executor::table_index_path(const std::string& db_name,const std::string& table_name) const{
        return database_path(db_name) / (table_name + ".idx");
    }
    void Executor::save_catalog() const{
        try{
            catalog_.save_to_file(catalog_path_);
        } catch (const std::exception& exception){
            throw ExecutionError("failed to save catalog to file: " + catalog_path_.string() + ": " + exception.what());
        }
    }
    ExecutionResult Executor::execute(const sql::SQLCommand& command){
        try{
            //dispatch command to corresponding executor function
            switch(command.type){
                case sql::CommandType::CREATE_DATABASE:
                    return execute_create_database(command);
                case sql::CommandType::DROP_DATABASE:
                    return execute_drop_database(command);
                case sql::CommandType::USE_DATABASE:
                    return execute_use_database(command);
                case sql::CommandType::SHOW_DATABASES:
                    return execute_show_databases(command);
                case sql::CommandType::CREATE_TABLE:
                    return execute_create_table(command);
                case sql::CommandType::DROP_TABLE:
                    return execute_drop_table(command);
                case sql::CommandType::SHOW_TABLES:
                    return execute_show_tables(command);
                case sql::CommandType::INSERT:
                case sql::CommandType::DELETE:
                case sql::CommandType::UPDATE:
                case sql::CommandType::SELECT:
                case sql::CommandType::ALTER_TABLE:
                    throw ExecutionError("Command type not implemented yet");
            }
            throw ExecutionError("Unknown command type");
        }catch (const ExecutionError&){
            throw;
        }catch (const std::exception& exception){
            throw ExecutionError("Execution error: " + std::string(exception.what()));
        }
    }
    ExecutionResult Executor::execute_create_database(const sql::SQLCommand& command){
        const std::string& db_name = command.database_name;
        const std::filesystem::path db_path = database_path(db_name);
        bool catalog_added = false;
        bool directory_created = false;
        try{
            catalog_.create_database(db_name);
            catalog_added = true;
            std::error_code error;
            directory_created = std::filesystem::create_directory(db_path,error);
            if(error){
                throw ExecutionError("failed to create database directory: " + db_path.string() + ": " + error.message());
            }
            //duplicate check for directory creation
            if(!directory_created){
                throw ExecutionError("database directory already exists: " + db_path.string());
            }
            save_catalog();
        }catch (...){
            //rollback if any error occurs
            if(catalog_added){
                try{
                    catalog_.drop_database(db_name);
                } catch (...){
                    
                }
            }
            if(directory_created){
                std::error_code error;
                std::filesystem::remove(db_path,error);
            }
            throw;
        }
        ExecutionResult res{};
        res.message = "Database created successfully: " + db_name; 
        return res;
    }
    ExecutionResult Executor::execute_use_database(const sql::SQLCommand& command) {
        const std::string& db_name = command.database_name;
        if(!catalog_.get_database_id(db_name).has_value()){
            throw ExecutionError("Database does not exist: " + db_name);
        }
        now_database_ = db_name;
        ExecutionResult res{};
        res.message = "Using database: " + db_name;
        return res;
    }
    ExecutionResult Executor::execute_show_databases(const sql::SQLCommand&){
        ExecutionResult res{};
        res.columns.push_back("Database");
        for(const auto& db : catalog_.get_databases()){
            res.rows.push_back({db.get_name()});
        }
        res.message = std::to_string(res.rows.size()) + " databases";
        return res;
    }
    ExecutionResult Executor::execute_drop_database(const sql::SQLCommand& command){
        const std::string& db_name = command.database_name;
        if(!catalog_.get_database_id(db_name).has_value()){
            throw ExecutionError("Database does not exist: " + db_name);
        }
        const std::filesystem::path db_path = database_path(db_name);
        //create a temporary directory to move the database directory to
        std::filesystem::path tmp_path = db_path;
        tmp_path += ".drop_tmp";
        std::error_code error;
        const bool tmp_exists = std::filesystem::exists(tmp_path,error);
        if(error){
            throw ExecutionError("failed to check if temporary drop directory exists: " + tmp_path.string() + ": " + error.message());
        }
        if(tmp_exists){
            throw ExecutionError("temporary drop directory already exists: " + tmp_path.string());
        }
        const Catalog catalog_backup = catalog_;
        //rename the database directory to the temporary directory
        std::filesystem::rename(db_path,tmp_path,error);
        if(error){
            throw ExecutionError("failed to stage database directory '" + db_path.string() + "' for deletion: " + error.message());
        }
        try{
            catalog_.drop_database(db_name);
            save_catalog();
        }catch (const std::exception& exception){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::error_code rollback_error;
            //recover the database directory
            std::filesystem::rename(tmp_path,db_path,rollback_error);
            if(rollback_error){
                throw ExecutionError("failed to drop database: " + std::string(exception.what()) + " and failed to rollback database directory: " + rollback_error.message());
            }
            throw;
        } 
        //if database is being used now
        if(now_database_.has_value() && now_database_.value() == db_name){
            now_database_.reset();
        }
        error.clear();
        std::filesystem::remove_all(tmp_path,error);
        if(error){
            throw ExecutionError("Database was removed from the catalog, but its staged directory could not be deleted: " + tmp_path.string() +": " + error.message());
        }
        ExecutionResult res{};
        res.message = "Database dropped successfully: " + db_name;
        return res;
    }
    ExecutionResult Executor::execute_create_table(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
        std::error_code error;
        //check if table file already exists
        const bool data_exists = std::filesystem::exists(data_path,error);
        if(error){
            throw ExecutionError("failed to check if table data file exists: " + data_path.string() + ": " + error.message());
        }
        if(data_exists){
            throw ExecutionError("table data file already exists: " + data_path.string());
        }
        const bool index_exists = std::filesystem::exists(index_path,error);
        if(error){
            throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
        }
        if(index_exists){
            throw ExecutionError("table index file already exists: " + index_path.string());
        }
        //create Column from sqlcommand
        std::vector<Column> columns;
        columns.reserve(command.columns.size());
        for(const auto& definiton : command.columns){
            columns.emplace_back(definiton.name,definiton.type,definiton.is_primary_key);
        }
        Table table{command.table_name,std::move(columns)};
        std::optional<DataType> pk_type;
        const std::optional<std::size_t> pk_id = table.get_pk_id();
        if(pk_id.has_value()){
            pk_type = table.get_column(pk_id.value()).get_type();
        }
        const Catalog catalog_backup = catalog_;
        try {
            //create file of the table
            catalog_.create_table(db_name,std::move(table));
            TableFile{data_path};
            if(pk_type.has_value()){
                BPlusTree{index_path,pk_type.value()};
            }
            save_catalog();
        } catch(...){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::error_code rollback_error;
            std::filesystem::remove(data_path,rollback_error);
            std::filesystem::remove(index_path,rollback_error);
            throw;
        }
        ExecutionResult res{};
        res.message = "Table created successfully: " + command.table_name;
        return res;
    }
    ExecutionResult Executor::execute_show_tables(const sql::SQLCommand&){
        const std::string& db_name = require_now_database();
        const Database& database = catalog_.get_database(db_name);
        ExecutionResult res{};
        res.columns.push_back("Table");
        //get table names from catalog
        for(const auto& table : database.get_tables()){
            res.rows.push_back({table.get_name()});
        }
        res.message = std::to_string(res.rows.size()) + " tables in database '" + db_name + "'";
        return res;
    }
    ExecutionResult Executor::execute_drop_table(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        const Database& database = catalog_.get_database(db_name);
        const std::optional<std::size_t> table_id = database.get_table_id(command.table_name);
        //existence check
        if(!table_id.has_value()){
            throw ExecutionError("Table does not exist: " + command.table_name);
        }
        const Table& table = database.get_table(command.table_name);
        const bool have_index = table.get_pk_id().has_value();
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
        //copy temporary paths for rollback
        std::filesystem::path tmp_data_path = data_path;
        tmp_data_path += ".drop_tmp";
        std::filesystem::path tmp_index_path = index_path;
        tmp_index_path += ".drop_tmp";
        std::error_code error;
        const bool data_exists = std::filesystem::exists(data_path,error);
        if(error){
            throw ExecutionError("failed to check if table data file exists: " + data_path.string() + ": " + error.message());
        }
        if(!data_exists){
            throw ExecutionError("table data file does not exist: " + data_path.string());
        }
        const bool index_exists = std::filesystem::exists(index_path,error);
        if(error){
            throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
        }
        if(have_index && !index_exists){
            throw ExecutionError("primary key index file does not exist: " + index_path.string());
        }
        //check if temporary files already exist
        const bool tmp_data_exists = std::filesystem::exists(tmp_data_path,error);
        if(error){
            throw ExecutionError("failed to check if temporary data file exists: " + error.message());
        }
        if(tmp_data_exists){
            throw ExecutionError("temporary table data file already exists: " + tmp_data_path.string());
        }
        if(index_exists){
            const bool tmp_index_exists = std::filesystem::exists(tmp_index_path,error);
            if(error){
                throw ExecutionError("failed to check if temporary index file exists: " + error.message());
            }
            if(tmp_index_exists){
                throw ExecutionError("temporary table index file already exists: " + tmp_index_path.string());
            }
        }
        const Catalog catalog_backup = catalog_;
        bool data_staged = false;
        bool index_staged = false;
        try{
            std::filesystem::rename(data_path,tmp_data_path,error);
            if(error){
                throw ExecutionError("failed to stage table data file: " + error.message());
            }
            data_staged = true;
            if(index_exists){
                error.clear();
                std::filesystem::rename(index_path,tmp_index_path,error);
                if(error){
                    throw ExecutionError("failed to stage table index file: " + error.message());
                }
                index_staged = true;
            }
            catalog_.drop_table(db_name,command.table_name);
            save_catalog();
        }catch(const std::exception& exception){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::string errors;
            if(index_staged){
                std::error_code rollback_error;
                std::filesystem::rename(tmp_index_path,index_path,rollback_error);
                if(rollback_error){
                    errors += "index rollback failed: " + rollback_error.message() + "; ";
                }
            }
            if(data_staged){
                std::error_code rollback_error;
                std::filesystem::rename(tmp_data_path,data_path,rollback_error);
                if(rollback_error){
                    errors += "data rollback failed: " + rollback_error.message() + "; ";
                }
            }
            if(!errors.empty()){
                throw ExecutionError("failed to drop table: " + std::string(exception.what()) + "; " + errors);
            }
            throw;
        }
        //delete the staged files
        error.clear();
        std::filesystem::remove(tmp_data_path,error);
        if(error){
            throw ExecutionError("Table was removed from the catalog, but its staged data file could not be deleted: "+ error.message());
        }
        if(index_staged){
            error.clear();
            std::filesystem::remove(tmp_index_path,error);
            if(error){
                throw ExecutionError("Table was removed from the catalog, but its staged index file could not be deleted: "+ error.message());
            }
        }
        ExecutionResult res{};
        res.message = "Table dropped successfully: " + command.table_name;
        return res;
    }
}