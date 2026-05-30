#pragma once
#include "Pager.h"
#include "Serializer.h"
#include "core/Table.h"
#include <string>
#include <cstdint>

namespace dbms {

class TableFile {
private:
    std::fstream file_;
    std::string filename_;

public:
    TableFile(const std::string& filename) : filename_(filename) {
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            file_.open(filename, std::ios::out | std::ios::binary);
            file_.close();
            file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
    }

    ~TableFile() {
        if (file_.is_open()) file_.close();
    }

    uint32_t insert_row(const Row& row) {
        file_.seekp(0, std::ios::end);
        uint32_t offset = file_.tellp();
        
        char is_deleted = 0;
        file_.write(&is_deleted, 1);
        
        uint16_t num_cols = row.values.size();
        file_.write(reinterpret_cast<const char*>(&num_cols), sizeof(num_cols));
        
        for (size_t i = 0; i < row.values.size(); ++i) {
            const Value& val = row.values[i];
            char type = val.type == DataType::INT ? 0 : (val.type == DataType::STRING ? 1 : 2);
            file_.write(&type, 1);
            if (val.type == DataType::INT) {
                file_.write(reinterpret_cast<const char*>(&val.int_val), sizeof(int));
            } else if (val.type == DataType::FLOAT) {
                file_.write(reinterpret_cast<const char*>(&val.float_val), sizeof(double));
            } else {
                uint16_t len = val.str_val.length();
                file_.write(reinterpret_cast<const char*>(&len), sizeof(uint16_t));
                file_.write(val.str_val.c_str(), len);
            }
        }
        file_.flush();
        return offset;
    }

    bool get_row(uint32_t offset, Row& row) {
        file_.seekg(offset, std::ios::beg);
        char is_deleted;
        if (!file_.read(&is_deleted, 1)) return false; // EOF or error
        if (is_deleted == 1) return false; // Row is deleted
        
        uint16_t num_cols;
        file_.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));
        
        row.values.clear();
        for (uint16_t i = 0; i < num_cols; ++i) {
            char type;
            file_.read(&type, 1);
            if (type == 0) {
                int val;
                file_.read(reinterpret_cast<char*>(&val), sizeof(int));
                row.values.push_back(Value(val));
            } else if (type == 2) {
                double val;
                file_.read(reinterpret_cast<char*>(&val), sizeof(double));
                row.values.push_back(Value(val));
            } else {
                uint16_t len;
                file_.read(reinterpret_cast<char*>(&len), sizeof(uint16_t));
                char buf[256];
                if (len > 0) file_.read(buf, len);
                row.values.push_back(Value(std::string(buf, len)));
            }
        }
        return true;
    }

    void delete_row(uint32_t offset) {
        file_.seekp(offset, std::ios::beg);
        char is_deleted = 1;
        file_.write(&is_deleted, 1);
        file_.flush();
    }
    
    // Scan all rows. Returns true and populates vector, ignoring deleted rows.
    void scan_all_rows(Vector<Row>& out_rows, Vector<uint32_t>& out_offsets) {
        file_.seekg(0, std::ios::beg);
        while (true) {
            uint32_t offset = file_.tellg();
            char is_deleted;
            if (!file_.read(&is_deleted, 1)) break;
            
            uint16_t num_cols;
            file_.read(reinterpret_cast<char*>(&num_cols), sizeof(num_cols));
            
            Row row;
            for (uint16_t i = 0; i < num_cols; ++i) {
                char type;
                file_.read(&type, 1);
                if (type == 0) {
                    int val;
                    file_.read(reinterpret_cast<char*>(&val), sizeof(int));
                    row.values.push_back(Value(val));
                } else if (type == 2) {
                    double val;
                    file_.read(reinterpret_cast<char*>(&val), sizeof(double));
                    row.values.push_back(Value(val));
                } else {
                    uint16_t len;
                    file_.read(reinterpret_cast<char*>(&len), sizeof(uint16_t));
                    char buf[256];
                    if (len > 0) file_.read(buf, len);
                    row.values.push_back(Value(std::string(buf, len)));
                }
            }
            
            if (is_deleted == 0) {
                out_rows.push_back(std::move(row));
                out_offsets.push_back(offset);
            }
        }
        file_.clear();
    }
};

} // namespace dbms
