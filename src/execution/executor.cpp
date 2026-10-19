#include "execution/executor.hpp"
#include "storage/table_file.hpp"
#include "storage/b_plus_tree.hpp"
#include <system_error>
#include <utility>
#include <set>
#include <map>

namespace minidb {
    namespace {
        const char* data_type_name(DataType type){
            switch(type){
                case DataType::INT:
                    return "INT";
                case DataType::FLOAT:
                    return "FLOAT";
                case DataType::STRING:
                    return "STRING";
            }
            throw std::logic_error("Unknown DataType");
        }
        const char* aggregate_function_name(sql::AggregateType type){
            switch(type){
                case sql::AggregateType::COUNT:
                    return "COUNT";
                case sql::AggregateType::AVG:
                    return "AVG";
                case sql::AggregateType::MIN:
                    return "MIN";
                case sql::AggregateType::MAX:
                    return "MAX";
                case sql::AggregateType::NONE:
                    return "NONE";
            }
            throw std::logic_error("Unknown AggregateType");
        }
    }
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
            reload_catalog();
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
                    return execute_insert(command);
                case sql::CommandType::DELETE:
                    return execute_delete(command);
                case sql::CommandType::UPDATE:
                    return execute_update(command);
                case sql::CommandType::SELECT:
                    return execute_select(command);
                case sql::CommandType::ALTER_TABLE:
                    return execute_alter_table(command);
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
                    throw ExecutionError("failed to create database: " + db_name + " and failed to rollback catalog");
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
    //check if input is legal and handle type conversion
    std::vector<Value> Executor::normalize_insert_values(const Table& table,const std::vector<Value>& input) const{
        if(input.size() != table.get_column_cnt()){
            throw ExecutionError("Number of values does not match number of columns in table '" + table.get_name() + "'");
        }
        std::vector<Value> res;
        res.reserve(input.size());
        for(std::size_t i = 0; i < input.size(); ++i){
            const Column& column = table.get_column(i);
            const Value& value = input[i];
            const DataType column_type = column.get_type();
            const DataType value_type = value.get_type();
            if(column_type == value_type){
                res.push_back(value);
                continue;
            }
            //INT must be converted to FLOAT safely
            if(column_type == DataType::FLOAT && value_type == DataType::INT){
                res.emplace_back(static_cast<double>(value.as_int()));
                continue;
            }
            throw ExecutionError("Type mismatch for column '" + column.get_name() + "': expected " + data_type_name(column_type) + ", got " + data_type_name(value_type));
        }
        return res;
    }
    //execute an INSERT command
    ExecutionResult Executor::execute_insert(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        const Table& table = catalog_.get_database(db_name).get_table(command.table_name);
        //get normalized values
        std::vector<Value> normalized_values = normalize_insert_values(table,command.values);
        Row row{std::move(normalized_values)};
        //get table data path
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        std::error_code error;
        const bool data_exists = std::filesystem::exists(data_path,error);
        if(error){
            throw ExecutionError("failed to check if table data file exists: " + data_path.string() + ": " + error.message());
        }
        if(!data_exists){
            throw ExecutionError("table data file does not exist: " + data_path.string());
        }
        TableFile table_file{data_path};
        const std::optional<std::size_t> pk_id = table.get_pk_id();
        if(!pk_id.has_value()){
            //only insert into .dat file
            static_cast<void>(table_file.insert_row(row));
        }else{
            const std::size_t key_id = pk_id.value();
            const Column& pk_column = table.get_column(key_id);
            const Value& pk = row.get_value(key_id);
            const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
            error.clear();
            const bool index_exists = std::filesystem::exists(index_path,error);
            if(error){
                throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
            }
            if(!index_exists){
                throw ExecutionError("table index file does not exist: " + index_path.string());
            }
            BPlusTree index{index_path,pk_column.get_type()};
            // check for duplicate primary key before inserting
            if(index.search(pk).has_value()){
                throw ExecutionError("Duplicate primary key value for column '" + pk_column.get_name() + "': " + pk.to_string());
            }
            //insert will return the row offset of the inserted row in the .dat file
            const TableFile::Row_offset row_offset = table_file.insert_row(row);
            try{
                //insert into B+ tree
                index.insert(pk,row_offset);
            }catch(const std::exception& exception) {
                try {
                    //rollback the .dat file if B+ tree insertion fails
                    table_file.delete_row(row_offset);
                }catch(const std::exception& rollback_exception){
                    throw ExecutionError("Failed to insert into B+ tree: " + std::string(exception.what()) + "; Failed to rollback .dat file: " + rollback_exception.what());
                }
                throw ExecutionError("Failed to insert into B+ tree: " + std::string(exception.what()));
            }
        }
        ExecutionResult res{};
        res.affected_rows = 1;
        res.message = "1 row inserted successfully into table '" + table.get_name() + "'";
        return res;
    }
    std::vector<Executor::parsedCondition> Executor::parse_conditions(const Table& table,const sql::SQLCommand& command) const{
        //no conditions in SELECT
        if(command.conditions.empty()){
            if(!command.logical_operators.empty()){
                throw ExecutionError("Logical operators provided without any conditions");
            }
            return {};
        }
        if(command.logical_operators.size() + 1 != command.conditions.size()){
            throw ExecutionError("Invalid WHERE condition chain");
        }
        std::vector<parsedCondition> res;
        res.reserve(command.conditions.size());
        for(const auto& condition : command.conditions){
            //convert column name to column id
            const std::optional<std::size_t> column_id = table.get_column_id(condition.column_name);
            if(!column_id.has_value()){
                throw ExecutionError("Column does not exist: " + condition.column_name);
            }
            const Column& column = table.get_column(column_id.value());
            const DataType expected_type = column.get_type();
            const DataType actual_type = condition.value.get_type();
            if(expected_type == actual_type){
                res.emplace_back(
                    column_id.value(),
                    condition.comparison,
                    condition.value
                );
                continue;
            }
            //FLOAT can be compared with INT
            if(expected_type == DataType::FLOAT && actual_type == DataType::INT){
                res.emplace_back(
                    column_id.value(),
                    condition.comparison,
                    Value(static_cast<double>(condition.value.as_int()))
                );
                continue;
            }
            throw ExecutionError("Type mismatch for column '" + column.get_name() + "': expected " + data_type_name(expected_type) + ", got " + data_type_name(actual_type));
        }
        return res;
    }
    bool Executor::row_matches_conditions(
        const Row& row,
        const std::vector<parsedCondition>& conditions,
        const std::vector<sql::LogicalOperator>& logical_operators
    )const {
        if(conditions.empty()){
            return true;
        }
        if(logical_operators.size() + 1 != conditions.size()){
            throw ExecutionError("Invalid logical operator chain");
        }
        //temporary function to evaluate a single condition 
        const auto evaluate = [&row](const parsedCondition& condition){
            const Value& row_value = row.get_value(condition.column_id);
            switch(condition.comparison){
                case sql::ComparisonOperator::EQUAL:
                    return row_value == condition.value;
                case sql::ComparisonOperator::LESS:
                    return row_value < condition.value;
                case sql::ComparisonOperator::GREATER:
                    return row_value > condition.value;
            }
            throw ExecutionError("Unknown comparison operator");
        };
        //evaluate from left to right
        bool res = evaluate(conditions[0]);
        for(std::size_t i = 0; i < logical_operators.size(); ++i){
            const bool next = evaluate(conditions[i + 1]);
            if(logical_operators[i] == sql::LogicalOperator::AND){
                res = res && next;
            }else if(logical_operators[i] == sql::LogicalOperator::OR){
                res = res || next;
            }else{
                throw ExecutionError("Unknown logical operator");
            }
        }
        return res;
    }
    std::vector<std::size_t> Executor::parse_projection_columns(
        const Table& table,
        const sql::SQLCommand& command
    ) const{
        if(command.select_items.empty()){
            throw ExecutionError("No columns specified in SELECT");
        }
        bool contains_all = false;
        for(const auto& item : command.select_items){
            if(item.type == sql::SelectType::ALL){
                contains_all = true;
                break;
            }
        }
        //there is *
        if(contains_all){
            if(command.select_items.size() != 1){
                throw ExecutionError("Cannot combine SELECT * with other SELECT items");
            }
            std::vector<std::size_t> res;
            res.reserve(table.get_column_cnt());
            for(std::size_t i = 0; i < table.get_column_cnt(); ++i){
                res.push_back(i);
            }
            return res;
        }
        std::vector<std::size_t> res;
        res.reserve(command.select_items.size());
        for(const auto& item : command.select_items){
            //aggregate functions are not supported yet
            if(item.type == sql::SelectType::AGGREGATE){
                throw ExecutionError("Aggregate functions are not supported yet");
            }
            //select certain column
            if(item.type != sql::SelectType::COLUMN){
                throw ExecutionError("Unknown SELECT item type");
            }
            const std::optional<std::size_t> column_id = table.get_column_id(item.column_name);
            if(!column_id.has_value()){
                throw ExecutionError("Column does not exist: " + item.column_name);
            }
            res.push_back(column_id.value());
        }
        return res;
    }
    ExecutionResult Executor::execute_select(const sql::SQLCommand& command) const{
        const std::string& db_name = require_now_database();
        const Table& table = catalog_.get_database(db_name).get_table(command.table_name);
        //check if the select command is an aggregate query
        bool is_aggregate_query = command.group_by_column.has_value();
        for(const sql::SelectItem& item : command.select_items){
            if(item.type == sql::SelectType::AGGREGATE){
                is_aggregate_query = true;
                break;
            }
        }
        std::vector<std::size_t> projection_columns;
        std::optional<aggregatePlan> aggregate_plan;
        if(is_aggregate_query){
            aggregate_plan = create_aggregate_plan(table,command);
        }else{
            projection_columns = parse_projection_columns(table,command);
        }
        //parse conditions
        const std::vector<parsedCondition> conditions = parse_conditions(table,command);
        const std::filesystem::path path = table_path(db_name,command.table_name);
        std::error_code error;
        bool path_exists = std::filesystem::exists(path,error);
        if(error){
            throw ExecutionError("failed to check if table data file exists: " + path.string() + ": " + error.message());
        }
        if(!path_exists){
            throw ExecutionError("Table data file does not exist: " + path.string());
        }
        //open file
        TableFile table_file{path};
        std::vector<TableFile::row_entry> entries;
        ExecutionResult res{};
        const std::optional<std::size_t> index_condition_id = find_pk_equality_condition(table,conditions,command.logical_operators);
        //the select command can be used for B+ tree search
        if(index_condition_id.has_value()){
            //sign of select type
            res.select_path = selectPath::PK_INDEX_SCAN;
            const std::size_t pk_id = table.get_pk_id().value();
            const Column& pk_column = table.get_column(pk_id);
            const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
            error.clear();
            const bool index_exists = std::filesystem::exists(index_path,error);
            if(error){
                throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
            }
            if(!index_exists){
                throw ExecutionError("table index file does not exist: " + index_path.string());
            }
            //open B+ tree index file
            BPlusTree index{index_path,pk_column.get_type()};
            const Value& key_value = conditions[index_condition_id.value()].value;
            const std::optional<BPlusTree::row_offset> row_offset = index.search(key_value);
            if(row_offset.has_value()){
                entries.emplace_back(
                    row_offset.value(),
                    table_file.get_row(row_offset.value())
                );
            }
        }else{
            //sign of select type
            res.select_path = selectPath::FULL_TABLE_SCAN;
            //scan all rows in the table file
            entries = table_file.scan_all_rows();
        }
        std::vector<const Row*> matched_rows;
        matched_rows.reserve(entries.size());
        for(const auto& entry : entries){
            if(row_matches_conditions(
                entry.row,
                conditions,
                command.logical_operators)){
                matched_rows.push_back(&entry.row);
            }
        }
        if(aggregate_plan.has_value()){
            //execute aggregate query
            const aggregatePlan& plan = aggregate_plan.value();
            //get column names in result table
            for(const parsedSelectItem& item : plan.items){
                res.columns.push_back(item.output_name);
            }
            //NO GROUP BY
            if(!plan.group_by_column_id.has_value()){
                std::vector<std::string> output_row;
                output_row.reserve(plan.items.size());
                for(const parsedSelectItem& item : plan.items){
                    if(item.aggregate_type == sql::AggregateType::NONE){
                        throw ExecutionError("no aggregate function in query");
                    }
                    output_row.push_back(calculate_aggregate(item,matched_rows));
                }
                res.rows.push_back(std::move(output_row));
                res.message = std::to_string(res.rows.size()) + " rows selected from table '" + table.get_name() + "'";
                return res;
            }
            //GROUP BY
            const std::size_t group_column_id = plan.group_by_column_id.value();
            /*  key = group by column value
                value = all rows belonging to this group  */
            std::map<Value,std::vector<const Row*>> groups;
            for(const Row* row : matched_rows){
                const Value& group_key = row->get_value(group_column_id);
                groups[group_key].push_back(row);
            }
            for(const auto& [group_key,group_rows] : groups){
                std::vector<std::string> output_row;
                output_row.reserve(plan.items.size());
                for(const parsedSelectItem& item : plan.items){
                    if(item.aggregate_type == sql::AggregateType::NONE){
                        //group-by column
                        output_row.push_back(group_key.to_string());
                    }else{
                        output_row.push_back(calculate_aggregate(item,group_rows));
                    }
                }
                res.rows.push_back(std::move(output_row));
            }
            res.message = std::to_string(res.rows.size()) + " rows selected from table '" + table.get_name() + "'";
            return res;
        }
        //get column names in result table
        for(const std::size_t column_id : projection_columns){
            res.columns.push_back(table.get_column(column_id).get_name());
        }
        //entry = offset + row in file
        for(const Row* row : matched_rows){
            //create row in result table
            std::vector<std::string> output_row;
            output_row.reserve(projection_columns.size());
            for(const std::size_t column_id : projection_columns){
                output_row.push_back(row->get_value(column_id).to_string());
            }
            res.rows.push_back(std::move(output_row));
        }
        res.message = std::to_string(res.rows.size()) + " rows selected from table '" + table.get_name() + "'";
        return res;
    }
    std::optional<std::size_t> Executor::find_pk_equality_condition(
        const Table& table,
        const std::vector<parsedCondition>& conditions,
        const std::vector<sql::LogicalOperator>& logical_operators
    ) const{
        //check if there is a primary key
        const std::optional<std::size_t> pk_id = table.get_pk_id();
        if(!pk_id.has_value()){
            return std::nullopt;
        }
        //check if there is a OR operaotr
        for(const auto& op : logical_operators){
            if(op == sql::LogicalOperator::OR){
                return std::nullopt;
            }
        }
        //find condition like "pk = value"
        for(std::size_t i = 0; i < conditions.size(); ++i){
            if(conditions[i].column_id == pk_id.value() && conditions[i].comparison == sql::ComparisonOperator::EQUAL){
                return i;
            }
        }
        return std::nullopt;
    }
    ExecutionResult Executor::execute_delete(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        const Table& table = catalog_.get_database(db_name).get_table(command.table_name);
        const std::vector<parsedCondition> conditions = parse_conditions(table,command);
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        std::error_code error;
        const bool data_exists = std::filesystem::exists(data_path,error);
        if(error){
            throw ExecutionError("failed to check if table data file exists: " + data_path.string() + ": " + error.message());
        }
        if(!data_exists){
            throw ExecutionError("table data file does not exist: " + data_path.string());
        }
        //open file
        TableFile table_file{data_path};
        const std::optional<std::size_t> pk_id = table.get_pk_id();
        std::optional<BPlusTree> index;
        if(pk_id.has_value()){
            //construct B+ tree
            const Column& pk_column =table.get_column(pk_id.value());
            const std::filesystem::path index_path = table_index_path(db_name, command.table_name);
            error.clear();
            const bool index_exists = std::filesystem::exists(index_path,error);
            if(error){
                throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
            }
            if(!index_exists){
                throw ExecutionError("table index file does not exist: " + index_path.string());
            }
            index.emplace(index_path,pk_column.get_type());
        }
        std::vector<TableFile::row_entry> active_rows;
        const std::optional<std::size_t> index_condition_id = find_pk_equality_condition(table,conditions,command.logical_operators);
        if(index_condition_id.has_value()){
            //use index to find the row to delete
            const Value& key = conditions[index_condition_id.value()].value;
            const std::optional<BPlusTree::row_offset> row_offset = index->search(key);
            if(row_offset.has_value()){
                active_rows.emplace_back(
                    row_offset.value(),
                    table_file.get_row(row_offset.value())
                );
            }
        }else{
            active_rows = table_file.scan_all_rows();
        }
        std::vector<TableFile::row_entry> matched_rows;
        //get rows to delete
        for(const auto& entry : active_rows){
            if(row_matches_conditions(
                entry.row,
                conditions,
                command.logical_operators)){
                matched_rows.push_back(entry);
            }
        }
        std::size_t delete_cnt = 0;
        if(!pk_id.has_value()){
            //just make tomb if there is no primary key
            for(const auto& entry : matched_rows){
                table_file.delete_row(entry.offset);
                ++delete_cnt;
            }
        }else{
            for(const auto& entry : matched_rows){
                const Value& pk_value = entry.row.get_value(pk_id.value());
                //delete from B+ tree first
                const bool erased = index.value().erase(pk_value);
                if(!erased){
                    throw ExecutionError("Failed to delete primary key from B+ tree index for value: " + pk_value.to_string());
                }
                try{
                    table_file.delete_row(entry.offset);
                }catch (const std::exception& delete_exception){
                    //rollback the B+ tree deletion if .dat deletion fails
                    try{
                        index.value().insert(pk_value,entry.offset);
                    }catch (const std::exception& rollback_exception){
                        throw ExecutionError("Failed to delete row from .dat file: " + std::string(delete_exception.what()) + "; Failed to rollback B+ tree index: " + rollback_exception.what());
                    }
                    throw ExecutionError("Failed to delete row from .dat file: " + std::string(delete_exception.what()));
                }
                ++delete_cnt;
            }
        }
        ExecutionResult res{};
        res.affected_rows = delete_cnt;
        res.message = std::to_string(res.affected_rows) + " rows deleted from table '" + table.get_name() + "'";
        return res;
    }
    std::vector<Executor::parsedAssignment> Executor::parse_assignments(const Table& table,const sql::SQLCommand& command) const{
        if(command.assignments.empty()){
            throw ExecutionError("No assignments specified in UPDATE");
        }
        std::vector<parsedAssignment> res;
        res.reserve(command.assignments.size());
        std::vector<bool> assigned(table.get_column_cnt(),false);
        for(const auto& assignment : command.assignments){
            const std::optional<std::size_t> column_id = table.get_column_id(assignment.column_name);
            if(!column_id.has_value()){
                throw ExecutionError("column does not exist: " + assignment.column_name);
            }
            //check dupilicate assignment
            if(assigned[column_id.value()]){
                throw ExecutionError("column '" + assignment.column_name + "' is assigned multiple times");
            }
            assigned[column_id.value()] = true;
            const Column& column = table.get_column(column_id.value());
            const DataType expected_type = column.get_type();
            const DataType actual_type = assignment.value.get_type();
            if(expected_type == actual_type){
                res.emplace_back(column_id.value(),assignment.value);
                continue;
            }
            //FLOAT can be assigned from INT
            if(expected_type == DataType::FLOAT && actual_type == DataType::INT){
                res.emplace_back(column_id.value(),Value(static_cast<double>(assignment.value.as_int())));
                continue;
            }
            throw ExecutionError("incompatible types for column '" + assignment.column_name + "': expected " + data_type_name(expected_type) + ", got " + data_type_name(actual_type));
        }
        return res;
    }
    Row Executor::apply_assignments(
        const Row& row,
        const std::vector<parsedAssignment>& assignments
    ) const{
        std::vector<Value> new_values = row.get_values();
        for(const auto& assignment : assignments){
            new_values[assignment.column_id] = assignment.value;
        }
        return Row{std::move(new_values)};
    }
    ExecutionResult Executor::execute_update(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        const Table& table = catalog_.get_database(db_name).get_table(command.table_name);
        const std::vector<parsedAssignment> assignments = parse_assignments(table, command);
        const std::vector<parsedCondition> conditions = parse_conditions(table, command);
        const std::filesystem::path data_path = table_path(db_name, command.table_name);
        std::error_code error;
        const bool data_exists = std::filesystem::exists(data_path, error);
        if (error) {
            throw ExecutionError("failed to check table data file: " + data_path.string() + ": " + error.message());
        }
        if (!data_exists) {
            throw ExecutionError("table data file does not exist: " + data_path.string());
        }
        TableFile table_file{data_path};
        //open B+ tree index if primary key exists
        const std::optional<std::size_t> pk_id = table.get_pk_id();
        std::optional<BPlusTree> index;
        if(pk_id.has_value()){
            //construct B+ tree
            const Column& pk_cloumn = table.get_column(pk_id.value());
            const std::filesystem::path index_path = table_index_path(db_name, command.table_name);
            error.clear();
            const bool index_exists = std::filesystem::exists(index_path,error);
            if(error){
                throw ExecutionError("failed to check if table index file exists: " + index_path.string() + ": " + error.message());
            }
            if(!index_exists){
                throw ExecutionError("table index file does not exist: " + index_path.string());
            }
            index.emplace(index_path,pk_cloumn.get_type());
        }
        std::vector<TableFile::row_entry> candidate_rows;
        //use B+ tree index if possible
        const std::optional<std::size_t> index_conditon_id = find_pk_equality_condition(table,conditions,command.logical_operators);
        if(index_conditon_id.has_value()){
            const Value& key_value = conditions[index_conditon_id.value()].value;
            const std::optional<BPlusTree::row_offset> row_offset = index->search(key_value);
            if(row_offset.has_value()){
                candidate_rows.emplace_back(
                    row_offset.value(),
                    table_file.get_row(row_offset.value())
                );
            }
        }else{
            candidate_rows = table_file.scan_all_rows();
        }
        //create new rows
        struct updatingRow{
            TableFile::Row_offset old_offset;
            Row old_row;
            Row new_row;
        };
        std::vector<updatingRow> updating_rows;
        for(const auto& entry : candidate_rows){
            if(!row_matches_conditions(
                entry.row,
                conditions,
                command.logical_operators)){
                continue;
            }
            //new rows
            updating_rows.push_back({entry.offset,entry.row,apply_assignments(entry.row,assignments)});
        }
        if(pk_id.has_value()){
            std::set<Value> new_pk_values;
            for(const auto& updating_row : updating_rows){
                const Value& old_pk_value = updating_row.old_row.get_value(pk_id.value());
                const Value& new_pk_value = updating_row.new_row.get_value(pk_id.value());
                //check if the old offset and offset in index matches
                const std::optional<BPlusTree::row_offset> old_index_offset = index->search(old_pk_value);
                if(!old_index_offset.has_value() || old_index_offset.value() != updating_row.old_offset){
                    throw ExecutionError("Primary key index is inconsistent with table data for value: " + old_pk_value.to_string());
                }
                //check if the new primary key value is unique
                const bool inserted = new_pk_values.insert(new_pk_value).second;
                if(!inserted){
                    throw ExecutionError("Duplicate primary key value in UPDATE assignments: " + new_pk_value.to_string());
                }
                //check if the new primary key value already exists in the table
                const std::optional<BPlusTree::row_offset> existing_offset = index->search(new_pk_value);
                if(existing_offset.has_value() && existing_offset.value() != updating_row.old_offset){
                    throw ExecutionError("Duplicate primary key value in table for UPDATE assignments: " + new_pk_value.to_string());
                }
            }
        }
        std::size_t updated_cnt = 0;
        for(const auto& update : updating_rows){
            const TableFile::Row_offset new_offset = table_file.insert_row(update.new_row);
            bool old_index_removed = false;
            bool new_index_inserted = false;
            try{
                if(pk_id.has_value()){
                    const Value& old_pk_value = update.old_row.get_value(pk_id.value());
                    const Value& new_pk_value = update.new_row.get_value(pk_id.value());
                    //remove old primary key from B+ tree
                    const bool erased = index->erase(old_pk_value);
                    if(!erased){
                        throw ExecutionError("Failed to remove old primary key from B+ tree index for value: " + old_pk_value.to_string());
                    }
                    old_index_removed = true;
                    //insert new primary key into B+ tree
                    index->insert(new_pk_value,new_offset);
                    new_index_inserted = true;
                }
                table_file.delete_row(update.old_offset);
                ++updated_cnt;
            }catch (const std::exception& exception){
                //rollback if any error occurs
                std::error_code rollback_error;
                if(pk_id.has_value()){
                    const Value& old_pk_value = update.old_row.get_value(pk_id.value());
                    const Value& new_pk_value = update.new_row.get_value(pk_id.value());
                    if(new_index_inserted){
                        try{
                            const bool erased = index->erase(new_pk_value);
                            if(!erased){
                                throw ExecutionError("Failed to rollback B+ tree index for value: " + new_pk_value.to_string());
                            }
                        }catch(const std::exception& rollback_exception){
                            throw ExecutionError("Failed to update row: " + std::string(exception.what()) + "; Failed to rollback B+ tree index: " + rollback_exception.what());
                        }
                    }
                    if(old_index_removed){
                        try{
                            index->insert(old_pk_value,update.old_offset);
                        }catch(const std::exception& rollback_exception){
                            throw ExecutionError("Failed to update row: " + std::string(exception.what()) + "; Failed to rollback B+ tree index: " + rollback_exception.what());
                        }
                    }
                }
                try{
                    table_file.delete_row(new_offset);
                }catch(const std::exception& rollback_exception){
                    throw ExecutionError("Failed to update row: " + std::string(exception.what()) + "; Failed to rollback .dat file: " + rollback_exception.what());
                }
                throw ExecutionError("Failed to update row: " + std::string(exception.what()));
            }
        }
        ExecutionResult res{};
        res.affected_rows = updated_cnt;
        res.message = std::to_string(res.affected_rows) + " rows updated in table '" + table.get_name() + "'";
        return res;
    }
    Executor::aggregatePlan Executor::create_aggregate_plan(const Table& table,const sql::SQLCommand& command) const{
        if(command.select_items.empty()){
            throw ExecutionError("No columns specified in SELECT");
        }
        aggregatePlan plan{};
        //check if there is a aggregate function
        for(const auto& item : command.select_items){
            if(item.type == sql::SelectType::AGGREGATE){
                plan.has_aggregate = true;
            }
        }
        //convert column name to column id
        if(command.group_by_column.has_value()){
            const std::optional<std::size_t> group_by_column_id = table.get_column_id(command.group_by_column.value());
            if(!group_by_column_id.has_value()){
                throw ExecutionError("Group by column does not exist: " + command.group_by_column.value());
            }
            plan.group_by_column_id = group_by_column_id.value();
        }
        plan.items.reserve(command.select_items.size());
        for(const auto& item : command.select_items){
            //reject * with aggregate functions
            if(item.type == sql::SelectType::ALL){
                throw ExecutionError("SELECT * is not allowed with aggregate functions");
            }
            const std::optional<std::size_t> column_id = table.get_column_id(item.column_name);
            if(!column_id.has_value()){
                throw ExecutionError("Column does not exist: " + item.column_name);
            }
            const Column& column = table.get_column(column_id.value());
            if(item.type == sql::SelectType::COLUMN){
                //reject normal column when aggregate exists and group by does not exists
                if(plan.has_aggregate && !command.group_by_column.has_value()){
                    throw ExecutionError("Normal column '" + column.get_name() + "' is not allowed with aggregate functions when GROUP BY is not specified");
                }
                //if group by exists,the column must be the group by column
                if(plan.group_by_column_id.has_value() && column_id.value() != plan.group_by_column_id.value()){
                    throw ExecutionError("Normal column '" + column.get_name() + "' is not allowed with aggregate functions when GROUP BY is specified on a different column");
                }
                plan.items.emplace_back(column_id.value(),sql::AggregateType::NONE,column.get_name());
                continue;
            }
            if(item.type == sql::SelectType::AGGREGATE){
                if(item.aggregate == sql::AggregateType::NONE){
                    throw ExecutionError("Invalid aggregate function");
                }
                // STRING can only be used with COUNT aggregate function
                if(item.aggregate != sql::AggregateType::COUNT && column.get_type() == DataType::STRING){
                    throw ExecutionError("Aggregate function '" + std::string(aggregate_function_name(item.aggregate)) + "' is not allowed on STRING column '" + column.get_name() + "'");
                }
                const std::string output_name = std::string(aggregate_function_name(item.aggregate)) + "(" + column.get_name() + ")";
                plan.items.emplace_back(column_id.value(),item.aggregate,output_name);
                continue;
            }
            throw ExecutionError("Unknown SELECT item type");
        }
        return plan;
    }
    std::string Executor::calculate_aggregate(const parsedSelectItem& item,const std::vector<const Row*>& rows) const{
        //COUNT = number of rows
        if(item.aggregate_type == sql::AggregateType::COUNT){
            return std::to_string(rows.size());
        }
        if(rows.empty()){
            return "NULL";
        }
        //MAX
        if(item.aggregate_type == sql::AggregateType::MAX){
            const Value* max_value = &rows[0]->get_value(item.column_id);
            for(const Row* row : rows){
                const Value& value = row->get_value(item.column_id);
                if(value > *max_value){
                    max_value = &value;
                }
            }
            return max_value->to_string();
        }
        //MIN
        if(item.aggregate_type == sql::AggregateType::MIN){
            const Value* min_value = &rows[0]->get_value(item.column_id);
            for(const Row* row : rows){
                const Value& value = row->get_value(item.column_id);
                if(value < *min_value){
                    min_value = &value;
                }
            }
            return min_value->to_string();
        }
        //AVG
        if(item.aggregate_type == sql::AggregateType::AVG){
            double sum = 0.0;
            for(const Row* row : rows){
                const Value& value = row->get_value(item.column_id);
                if(value.get_type() == DataType::INT){
                    sum += static_cast<double>(value.as_int());
                }else if(value.get_type() == DataType::FLOAT){
                    sum += value.as_float();
                }else{
                    throw ExecutionError("AVG aggregate function is not allowed on STRING column");
                }
            }
            const double avg = sum / static_cast<double>(rows.size());
            return std::to_string(avg);
        }
        throw ExecutionError("Unknown aggregate function");
    }
    ExecutionResult Executor::execute_alter_table(const sql::SQLCommand& command){
        if(!command.alter_type.has_value()){
            throw ExecutionError("No column specified for ALTER TABLE");
        }
        switch(command.alter_type.value()){
            case sql::AlterType::RENAME_COLUMN:
                return execute_rename_column(command);
            case sql::AlterType::RENAME_TABLE:
                return execute_rename_table(command);
            case sql::AlterType::ADD_COLUMN:
                return execute_add_column(command);
            case sql::AlterType::DROP_COLUMN:
                return execute_drop_column(command);
        }
        throw ExecutionError("Unknown ALTER TABLE type");
    }
    ExecutionResult Executor::execute_rename_column(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        if(!command.target_column_name.has_value() || !command.new_name.has_value()){
            throw ExecutionError("No target column name specified for RENAME COLUMN");
        }
        const std::string& old_column_name = command.target_column_name.value();
        const std::string& new_column_name = command.new_name.value();
        const Table& old_table = catalog_.get_database(db_name).get_table(command.table_name);
        const std::optional<std::size_t> column_id = old_table.get_column_id(old_column_name);
        if(!column_id.has_value()){
            throw ExecutionError("Column does not exist: " + old_column_name);
        }
        //check if the new column name already exists
        const std::optional<std::size_t> exist_id = old_table.get_column_id(new_column_name);
        if(exist_id.has_value() && exist_id.value() != column_id.value()){
            throw ExecutionError("Column already exists: " + new_column_name);
        }
        std::vector<Column> new_columns = old_table.get_columns();
        const Column& old_column = new_columns[column_id.value()];
        //construct new column with the new name
        new_columns[column_id.value()] = Column{new_column_name,old_column.get_type(),old_column.is_primary_key()};
        Table replacement{old_table.get_name(),std::move(new_columns)};
        const Catalog catalog_backup = catalog_;
        try{
            catalog_.replace_table(db_name,old_table.get_name(),std::move(replacement));
            save_catalog();
        }catch(...){
            catalog_ = catalog_backup;
            throw;
        }
        ExecutionResult res{};
        res.message = "Column '" + old_column_name + "' renamed to '" + new_column_name + "' in table '" + old_table.get_name() + "'";
        return res;
    }
    ExecutionResult Executor::execute_rename_table(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        if(!command.new_name.has_value()){
            throw ExecutionError("No new table name specified for RENAME TABLE");
        }
        const std::string old_name = command.table_name;
        const std::string new_name = command.new_name.value();
        const Database& database = catalog_.get_database(db_name);
        const Table& old_table = database.get_table(old_name);
        //support renaming to the same name
        if(old_name == new_name){
            ExecutionResult res{};
            res.message = "Table '" + old_name + "' renamed to '" + new_name + "' in database '" + db_name + "'";
            return res;
        }
        if(database.get_table_id(new_name).has_value()){
            throw ExecutionError("Table already exists: " + new_name);
        }
        //construct replacement
        Table replacement{new_name,old_table.get_columns()};
        const bool has_index = old_table.get_pk_id().has_value();
        const std::filesystem::path old_data_path = table_path(db_name,old_name);
        const std::filesystem::path new_data_path = table_path(db_name,new_name);
        const std::filesystem::path old_index_path = table_index_path(db_name,old_name);
        const std::filesystem::path new_index_path = table_index_path(db_name,new_name);
        //temparory function to check if a path exists
        const auto path_exists = [](const std::filesystem::path& path){
            std::error_code error;
            const bool exists = std::filesystem::exists(path,error);
            if(error){
                throw ExecutionError("failed to check if file exists: " + path.string() + ": " + error.message());
            }
            return exists;
        };
        if(!path_exists(old_data_path)){
            throw ExecutionError("Table data file does not exist: " + old_data_path.string());
        }
        if(path_exists(new_data_path)){
            throw ExecutionError("Table data file already exists: " + new_data_path.string());
        }
        if(has_index && !path_exists(old_index_path)){
            throw ExecutionError("Table index file does not exist: " + old_index_path.string());
        }
        if(path_exists(new_index_path)){
            throw ExecutionError("Table index file already exists: " + new_index_path.string());
        }
        //backup catalog
        const Catalog catalog_backup = catalog_;
        bool data_renamed = false;
        bool index_renamed = false;
        try{
            std::error_code error;
            std::filesystem::rename(old_data_path,new_data_path,error);
            if(error){
                throw ExecutionError("Failed to rename table data file: " + old_data_path.string() + " to " + new_data_path.string() + ": " + error.message());
            }
            data_renamed = true;
            if(has_index){
                error.clear();
                std::filesystem::rename(old_index_path,new_index_path,error);
                if(error){
                    throw ExecutionError("Failed to rename table index file: " + old_index_path.string() + " to " + new_index_path.string() + ": " + error.message());
                }
                index_renamed = true;
            }
            catalog_.replace_table(db_name,old_name,std::move(replacement));
            save_catalog();
        }catch(const std::exception& exception){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::string rollback_error;
            const auto append_rollback_error = [&rollback_error](const std::string& msg){
                if(!rollback_error.empty()){
                    rollback_error += "; ";
                }
                rollback_error += msg;
            };
            std::error_code re;
            //recover data file
            if(index_renamed){
                std::filesystem::rename(new_index_path,old_index_path,re);
                if(re){
                    append_rollback_error("Failed to rollback table index file: " + re.message());
                }
            }
            re.clear();
            if(data_renamed){
                std::filesystem::rename(new_data_path,old_data_path,re);
                if(re){
                    append_rollback_error("Failed to rollback table data file: " + re.message());
                }
            }
            std::string msg = "Failed to rename table '" + old_name + "' to '" + new_name + "': " + exception.what();
            if(!rollback_error.empty()){
                msg += "; Rollback error: " + rollback_error;
            }
            throw ExecutionError(msg);
        }
        ExecutionResult res{};
        res.message = "Table '" + old_name + "' renamed to '" + new_name + "' in database '" + db_name + "'";
        return res;
    }
    ExecutionResult Executor::execute_add_column(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        if(!command.alter_column.has_value()){
            throw ExecutionError("No column specified for ADD COLUMN");
        }
        const sql::ColumnDefinition& definition = command.alter_column.value();
        const Table& old_table = catalog_.get_database(db_name).get_table(command.table_name);
        //check if the column already exists
        if(old_table.get_column_id(definition.name).has_value()){
            throw ExecutionError("Column already exists: " + definition.name);
        }
        std::vector<Column> new_columns = old_table.get_columns();
        new_columns.emplace_back(definition.name,definition.type,false);
        Table replacement{old_table.get_name(),std::move(new_columns)};
        //prepare default value for the every old row
        auto make_default_value = [&definition](){
            switch(definition.type){
                case DataType::INT:
                    return Value(0);
                case DataType::FLOAT:
                    return Value(0.0);
                case DataType::STRING:
                    return Value(std::string{});
            }
            throw ExecutionError("Unknown data type for ADD COLUMN ");
        };
        const std::optional<std::size_t> pk_id = old_table.get_pk_id();
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
        //create new data and index file paths
        std::filesystem::path new_data_path = data_path;
        new_data_path += ".alter_new";
        std::filesystem::path new_index_path = index_path;
        new_index_path += ".alter_new";
        const auto path_exists = [](const std::filesystem::path& path){
            std::error_code error;
            const bool exists = std::filesystem::exists(path,error);
            if(error){
                throw ExecutionError("failed to check if file exists: " + path.string() + ": " + error.message()); 
            }
            return exists;
        };
        if(!path_exists(data_path)){
            throw ExecutionError("Table data file does not exist: " + data_path.string());
        }
        if(pk_id.has_value() && !path_exists(index_path)){
            throw ExecutionError("Table index file does not exist: " + index_path.string());
        }
        if(path_exists(new_data_path)){
            throw ExecutionError("Temporary table data file already exists: " + new_data_path.string());
        }
        if(path_exists(new_index_path)){
            throw ExecutionError("Temporary table index file already exists: " + new_index_path.string());
        }
        //create temporary new data and index files
        std::size_t rewritten_rows = 0;
        //rename original files
        std::filesystem::path backup_data_path = data_path;
        backup_data_path += ".alter_old";
        std::filesystem::path backup_index_path = index_path;
        backup_index_path += ".alter_old";
        if(path_exists(backup_data_path)){
            throw ExecutionError(
            "Temporary backup data file already exists: " +
                backup_data_path.string()
            );
        }
        if(pk_id.has_value() && path_exists(backup_index_path)){
            throw ExecutionError(
            "Temporary backup index file already exists: " +
                backup_index_path.string()
            );
        }
        try{
            TableFile old_file{data_path};
            std::vector<TableFile::row_entry> old_rows = old_file.scan_all_rows();
            TableFile new_file{new_data_path};
            std::optional<BPlusTree> new_index;
            if(pk_id.has_value()){
                const DataType pk_type = old_table.get_column(pk_id.value()).get_type();
                new_index.emplace(new_index_path,pk_type);
            }
            for(const auto& entry : old_rows){
                //copy all values from old row
                std::vector<Value> new_values = entry.row.get_values();
                new_values.push_back(make_default_value());
                Row new_row{std::move(new_values)};
                //the offset can be diffferent between the old and new file
                //we must get the new offset and use it
                const TableFile::Row_offset new_offset = new_file.insert_row(new_row);
                if(new_index.has_value()){
                    const Value& pk_value = new_row.get_value(pk_id.value());
                    new_index->insert(pk_value,new_offset);
                }
                ++rewritten_rows;
            }
        }catch(const std::exception& exception){
            //rollback if any error occurs
            std::error_code rollback_error;
            std::filesystem::remove(new_data_path,rollback_error);
            if(rollback_error){
                throw ExecutionError("Failed to prepare files for ADD COLUMN: " + std::string(exception.what()) + "; Failed to remove temporary new table data file: " + rollback_error.message());
            }
            rollback_error.clear();
            std::filesystem::remove(new_index_path,rollback_error);
            if(rollback_error){
                throw ExecutionError("Failed to prepare files for ADD COLUMN: " + std::string(exception.what()) + "; Failed to remove temporary new table index file: " + rollback_error.message());
            }
            throw ExecutionError("Failed to prepare files for ADD COLUMN: " + std::string(exception.what()));
        }
        const Catalog catalog_backup = catalog_;
        bool old_data_backed_up = false;
        bool old_index_backed_up = false;
        bool new_data_installed = false;
        bool new_index_installed = false;
        try{
            //backup original files with renaming them
            std::error_code error;
            std::filesystem::rename(data_path,backup_data_path,error);
            if(error){
                throw ExecutionError("Failed to backup table data file: " + data_path.string() + " to " + backup_data_path.string() + ": " + error.message());
            }
            old_data_backed_up = true;
            if(pk_id.has_value()){
                error.clear();
                std::filesystem::rename(index_path,backup_index_path,error);
                if(error){
                    throw ExecutionError("Failed to backup table index file: " + index_path.string() + " to " + backup_index_path.string() + ": " + error.message());
                }
                old_index_backed_up = true;
            }
            // install new files by renaming them
            error.clear();
            std::filesystem::rename(new_data_path,data_path,error);
            if(error){
                throw ExecutionError("Failed to install new table data file: " + new_data_path.string() + " to " + data_path.string() + ": " + error.message());
            }
            new_data_installed = true;
            if(pk_id.has_value()){
                error.clear();
                std::filesystem::rename(new_index_path,index_path,error);
                if(error){
                    throw ExecutionError("Failed to install new table index file: " + new_index_path.string() + " to " + index_path.string() + ": " + error.message());
                }
                new_index_installed = true;
            }
            catalog_.replace_table(db_name,command.table_name,std::move(replacement));
            save_catalog();
        }catch(const std::exception& exception){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::string rollback_error;
            const auto append_rollback_error = [&rollback_error](const std::string& msg){
                if(!rollback_error.empty()){
                    rollback_error += "; ";
                }
                rollback_error += msg;
            };
            std::error_code re;
            //recover original files
            if(new_index_installed){
                std::filesystem::remove(index_path,re);
                if(re){
                    append_rollback_error("Failed to remove new table index file: " + re.message());
                }
            }
            re.clear();
            if(new_data_installed){
                std::filesystem::remove(data_path,re);
                if(re){
                    append_rollback_error("Failed to remove new table data file: " + re.message());
                }
            }
            re.clear();
            if(old_index_backed_up){
                std::filesystem::rename(backup_index_path,index_path,re);
                if(re){
                    append_rollback_error("Failed to restore original table index file: " + re.message());
                }
            }
            re.clear();
            if(old_data_backed_up){
                std::filesystem::rename(backup_data_path,data_path,re);
                if(re){
                    append_rollback_error("Failed to restore original table data file: " + re.message());
                }
            }
            re.clear();
            //remove temporary new files if they still exist
            std::filesystem::remove(new_data_path,re);
            if(re){
                append_rollback_error("Failed to remove temporary new table data file: " + re.message());
            }
            re.clear();
            std::filesystem::remove(new_index_path,re);
            if(re){
                append_rollback_error("Failed to remove temporary new table index file: " + re.message());
            }
            std::string msg = "Failed to add column '" + definition.name  + "' :" + exception.what();
            if(!rollback_error.empty()){
                msg += "; Rollback error: " + rollback_error;
            }
            throw ExecutionError(msg);
        }
        //remove backup files if everything is successful
        std::string cleanup_warning;
        std::error_code cleanup_error;
        std::filesystem::remove(backup_data_path,cleanup_error);
        if(cleanup_error){
            cleanup_warning += "Failed to remove backup table data file: " + cleanup_error.message();
        }
        if(pk_id.has_value()){
            cleanup_error.clear();
            std::filesystem::remove(backup_index_path,cleanup_error);
            if(cleanup_error){
                if(!cleanup_warning.empty()){
                    cleanup_warning += "; ";
                }
                cleanup_warning += "Failed to remove backup table index file: " + cleanup_error.message();
            }
        }
        ExecutionResult res{};
        res.affected_rows = rewritten_rows;
        res.message = "Column '" + definition.name + "' added to table '" + command.table_name + "';" + std::to_string(rewritten_rows) + " rows rewritten";
        if(!cleanup_warning.empty()){
            res.message += "; Warning: " + cleanup_warning;
        }
        return res;
    }
    ExecutionResult Executor::execute_drop_column(const sql::SQLCommand& command){
        const std::string& db_name = require_now_database();
        if(!command.target_column_name.has_value()){
            throw ExecutionError("No target column name specified for DROP COLUMN");
        }
        const std::string& column_name = command.target_column_name.value();
        const Table& old_table = catalog_.get_database(db_name).get_table(command.table_name);
        const std::optional<std::size_t> drop_column_id = old_table.get_column_id(column_name);
        if(!drop_column_id.has_value()){
            throw ExecutionError("Column does not exist: " + column_name);
        }
        const Column& drop_column = old_table.get_column(drop_column_id.value());
        //primary key column cannot be dropped
        if(drop_column.is_primary_key()){
            throw ExecutionError("Cannot drop primary key column: " + column_name);
        }
        //table must have at least 1 column
        if(old_table.get_columns().size() == 1){
            throw ExecutionError("Cannot drop the only column in the table: " + column_name);
        }
        std::vector<Column> new_columns = old_table.get_columns();
        new_columns.erase(new_columns.begin() + drop_column_id.value());
        Table replacement{old_table.get_name(),std::move(new_columns)};
        //pk id may change
        const std::optional<std::size_t> old_pk_id = old_table.get_pk_id();
        const std::optional<std::size_t> new_pk_id = replacement.get_pk_id();
        const std::filesystem::path data_path = table_path(db_name,command.table_name);
        const std::filesystem::path index_path = table_index_path(db_name,command.table_name);
        //create new data and index file paths
        std::filesystem::path new_data_path = data_path;
        new_data_path += ".alter_new";
        std::filesystem::path new_index_path = index_path;
        new_index_path += ".alter_new";
        const auto path_exists = [](const std::filesystem::path& path){
            std::error_code error;
            const bool exists = std::filesystem::exists(path,error);
            if(error){
                throw ExecutionError("failed to check if file exists: " + path.string() + ": " + error.message()); 
            }
            return exists;
        };
        if(!path_exists(data_path)){
            throw ExecutionError("Table data file does not exist: " + data_path.string());
        }
        if(old_pk_id.has_value() && !path_exists(index_path)){
            throw ExecutionError("Table index file does not exist: " + index_path.string());
        }
        if(path_exists(new_data_path)){
            throw ExecutionError("Temporary table data file already exists: " + new_data_path.string());
        }
        if(path_exists(new_index_path)){
            throw ExecutionError("Temporary table index file already exists: " + new_index_path.string());
        }
        //create temporary new data and index files
        std::size_t rewritten_rows = 0;
        //rename original files
        std::filesystem::path backup_data_path = data_path;
        backup_data_path += ".alter_old";
        std::filesystem::path backup_index_path = index_path;
        backup_index_path += ".alter_old";
        if(path_exists(backup_data_path)){
            throw ExecutionError(
            "Temporary backup data file already exists: " +
                backup_data_path.string()
            );
        }
        if(old_pk_id.has_value() && path_exists(backup_index_path)){
            throw ExecutionError(
            "Temporary backup index file already exists: " +
                backup_index_path.string()
            );
        }
        try{
            TableFile old_file{data_path};
            std::vector<TableFile::row_entry> old_rows = old_file.scan_all_rows();
            TableFile new_file{new_data_path};
            std::optional<BPlusTree> new_index;
            if(old_pk_id.has_value()){
                const DataType pk_type = old_table.get_column(old_pk_id.value()).get_type();
                new_index.emplace(new_index_path,pk_type);
            }
            for(const auto& entry : old_rows){
                //copy all values from old row
                std::vector<Value> new_values = entry.row.get_values();
                if(drop_column_id.value() >= new_values.size()){
                    throw ExecutionError("Invalid column id for DROP COLUMN");
                }
                new_values.erase(new_values.begin() + drop_column_id.value());
                Row new_row{std::move(new_values)};
                //the offset can be diffferent between the old and new file
                //we must get the new offset and use it
                const TableFile::Row_offset new_offset = new_file.insert_row(new_row);
                if(new_index.has_value()){
                    const Value& pk_value = new_row.get_value(new_pk_id.value());
                    new_index->insert(pk_value,new_offset);
                }
                ++rewritten_rows;
            }
        }catch(const std::exception& exception){
            //rollback if any error occurs
            std::error_code rollback_error;
            std::filesystem::remove(new_data_path,rollback_error);
            if(rollback_error){
                throw ExecutionError("Failed to prepare files for DROP COLUMN: " + std::string(exception.what()) + "; Failed to remove temporary new table data file: " + rollback_error.message());
            }
            rollback_error.clear();
            std::filesystem::remove(new_index_path,rollback_error);
            if(rollback_error){
                throw ExecutionError("Failed to prepare files for DROP COLUMN: " + std::string(exception.what()) + "; Failed to remove temporary new table index file: " + rollback_error.message());
            }
            throw ExecutionError("Failed to prepare files for DROP COLUMN: " + std::string(exception.what()));
        }
        const Catalog catalog_backup = catalog_;
        bool old_data_backed_up = false;
        bool old_index_backed_up = false;
        bool new_data_installed = false;
        bool new_index_installed = false;
        try{
            //backup original files with renaming them
            std::error_code error;
            std::filesystem::rename(data_path,backup_data_path,error);
            if(error){
                throw ExecutionError("Failed to backup table data file: " + data_path.string() + " to " + backup_data_path.string() + ": " + error.message());
            }
            old_data_backed_up = true;
            if(old_pk_id.has_value()){
                error.clear();
                std::filesystem::rename(index_path,backup_index_path,error);
                if(error){
                    throw ExecutionError("Failed to backup table index file: " + index_path.string() + " to " + backup_index_path.string() + ": " + error.message());
                }
                old_index_backed_up = true;
            }
            // install new files by renaming them
            error.clear();
            std::filesystem::rename(new_data_path,data_path,error);
            if(error){
                throw ExecutionError("Failed to install new table data file: " + new_data_path.string() + " to " + data_path.string() + ": " + error.message());
            }
            new_data_installed = true;
            if(old_pk_id.has_value()){
                error.clear();
                std::filesystem::rename(new_index_path,index_path,error);
                if(error){
                    throw ExecutionError("Failed to install new table index file: " + new_index_path.string() + " to " + index_path.string() + ": " + error.message());
                }
                new_index_installed = true;
            }
            catalog_.replace_table(db_name,command.table_name,std::move(replacement));
            save_catalog();
        }catch(const std::exception& exception){
            //rollback if any error occurs
            catalog_ = catalog_backup;
            std::string rollback_error;
            const auto append_rollback_error = [&rollback_error](const std::string& msg){
                if(!rollback_error.empty()){
                    rollback_error += "; ";
                }
                rollback_error += msg;
            };
            std::error_code re;
            //recover original files
            if(new_index_installed){
                std::filesystem::remove(index_path,re);
                if(re){
                    append_rollback_error("Failed to remove new table index file: " + re.message());
                }
            }
            re.clear();
            if(new_data_installed){
                std::filesystem::remove(data_path,re);
                if(re){
                    append_rollback_error("Failed to remove new table data file: " + re.message());
                }
            }
            re.clear();
            if(old_index_backed_up){
                std::filesystem::rename(backup_index_path,index_path,re);
                if(re){
                    append_rollback_error("Failed to restore original table index file: " + re.message());
                }
            }
            re.clear();
            if(old_data_backed_up){
                std::filesystem::rename(backup_data_path,data_path,re);
                if(re){
                    append_rollback_error("Failed to restore original table data file: " + re.message());
                }
            }
            re.clear();
            //remove temporary new files if they still exist
            std::filesystem::remove(new_data_path,re);
            if(re){
                append_rollback_error("Failed to remove temporary new table data file: " + re.message());
            }
            re.clear();
            std::filesystem::remove(new_index_path,re);
            if(re){
                append_rollback_error("Failed to remove temporary new table index file: " + re.message());
            }
            std::string msg = "Failed to drop column '" + column_name + "': " + exception.what();
            if(!rollback_error.empty()){
                msg += "; Rollback error: " + rollback_error;
            }
            throw ExecutionError(msg);
        }
        //remove backup files if everything is successful
        std::string cleanup_warning;
        std::error_code cleanup_error;
        std::filesystem::remove(backup_data_path,cleanup_error);
        if(cleanup_error){
            cleanup_warning += "Failed to remove backup table data file: " + cleanup_error.message();
        }
        if(old_pk_id.has_value()){
            cleanup_error.clear();
            std::filesystem::remove(backup_index_path,cleanup_error);
            if(cleanup_error){
                if(!cleanup_warning.empty()){
                    cleanup_warning += "; ";
                }
                cleanup_warning += "Failed to remove backup table index file: " + cleanup_error.message();
            }
        }
        ExecutionResult res{};
        res.affected_rows = rewritten_rows;
        res.message = "Column '" + column_name + "' dropped from table '" + command.table_name + "'; " + std::to_string(rewritten_rows) + " rows rewritten";
        if(!cleanup_warning.empty()){
            res.message += "; Warning: " + cleanup_warning;
        }
        return res;
    }
    void Executor::reload_catalog(){
        try{
            Catalog refreshed = Catalog::load_from_file(catalog_path_);
            if(now_database_.has_value() && !refreshed.get_database_id(now_database_.value()).has_value()){
                now_database_.reset();
            }
            catalog_ = std::move(refreshed);
        }catch(const std::exception& exception){
            throw ExecutionError("Failed to reload catalog: " + std::string(exception.what()));
        }
    }
}
