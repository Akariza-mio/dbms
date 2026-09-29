#include "storage/table_file.hpp"
#include "storage/serializer.hpp"
#include <stdexcept>
#include <system_error>
#include <array>
#include <limits>
#include <utility>

namespace minidb{
    TableFile::TableFile(const std::filesystem::path& path)
        : path_(path){
            std::error_code error;
            const bool is_exist = std::filesystem::exists(path_,error);
            if(error){
                throw std::runtime_error("Failed to check file existence: " + error.message());
            }
            //creator is only used to create the file
            if(!is_exist){
                std::ofstream creator(path_,std::ios::binary);
                if(!creator.is_open()){
                    throw std::runtime_error("Failed to create file: " + path_.string());
                }
            }
            fs.open(path_,std::ios::out | std::ios::in | std::ios::binary);
            if(!fs.is_open()){
                throw std::runtime_error("Failed to open table file: " + path_.string());
            }
    }
    TableFile::Row_offset TableFile::insert_row(const Row& row){
        serialization::byte_vector payload = serialization::serialize_row(row);
        if(payload.size() > std::numeric_limits<std::uint32_t>::max()){
            throw std::length_error("Row is too large");
        }
        //payload_size = value_cnt + each value's size
        const std::uint32_t payload_size = static_cast<std::uint32_t>(payload.size());
        //move the file pointer to the end of the file
        fs.seekp(0,std::ios::end);
        if(!fs){
            throw std::runtime_error("Failed to seek to the end of the table file: " + path_.string());
        }
        const std::streamoff pos = fs.tellp();
        if(pos < 0){
            throw std::runtime_error("Failed to get the row offset: " + path_.string());
        }
        //offset is the signal position of the inserting row 
        const Row_offset offset = static_cast<Row_offset>(pos);
        std::array<std::byte,HEADER_SIZE> header{};
        //construct the header
        header[0] = static_cast<std::byte>(ACTIVE);
        for (std::uint32_t i = 0; i < 4; ++i) {
            header[1 + i] = static_cast<std::byte>((payload_size >> (i * 8)) & 0xFFu);
        }
        //write header
        fs.write(reinterpret_cast<const char*>(header.data()),static_cast<std::streamsize>(header.size()));
        //write payload
        fs.write(reinterpret_cast<const char*>(payload.data()),static_cast<std::streamsize>(payload.size()));
        fs.flush();
        if(!fs){
            throw std::runtime_error("Failed to write row to the table file: " + path_.string());
        }
        return offset;
    }
    Row TableFile::get_row(Row_offset offset){
        //check if the offset is valid
        std::error_code error;
        const std::uintmax_t file_size = std::filesystem::file_size(path_,error);
        if(error){
            throw std::runtime_error("Failed to get the size of the table file: " + path_.string());
        }
        if(offset > file_size || HEADER_SIZE > file_size - offset){
            throw std::out_of_range("Row offset is out of range");
        }
        //move the file pointer to the offset of the row
        fs.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
        if(!fs){
            throw std::runtime_error("Failed to seek to the row offset: " + path_.string());
        }
        //read the header in file to array
        std::array<std::byte,HEADER_SIZE> header{};
        const std::streamsize header_size = static_cast<std::streamsize>(header.size());
        fs.read(reinterpret_cast<char*>(header.data()),header_size);
        if(!fs || fs.gcount() != header_size){
            throw std::runtime_error("Failed to read row header from the table file: " + path_.string());
        }
        //check the signal
        const std::uint8_t flag = std::to_integer<std::uint8_t>(header[0]);
        if(flag == DELETED){
            throw std::runtime_error("Row is deleted");
        }
        if(flag != ACTIVE){
            throw std::runtime_error("Invalid row flag");
        }
        //compute the payload size from the header
        std::uint32_t payload_size = 0;
        for (std::uint32_t i = 0; i < 4; ++i) {
            const std::uint32_t current = std::to_integer<std::uint8_t>(header[1 + i]);
            payload_size |= current << (i * 8);
        }
        const std::uintmax_t payload_offset = static_cast<std::uintmax_t>(offset) + HEADER_SIZE;
        if(payload_size > file_size - payload_offset){
            throw std::runtime_error("Truncated row payload");
        }
        //read the payload from the file
        serialization::byte_vector payload(payload_size);
        const std::streamsize payload_size_stream = static_cast<std::streamsize>(payload.size());
        fs.read(reinterpret_cast<char*>(payload.data()),payload_size_stream);
        if(!fs || fs.gcount() != payload_size_stream){
            throw std::runtime_error("Failed to read complete row payload");
        }
        return serialization::deserialize_row(payload);
    }
    void TableFile::delete_row(Row_offset offset){
        std::error_code error;
        const std::uintmax_t file_size = std::filesystem::file_size(path_,error);
        if(error){
            throw std::runtime_error("Failed to get the size of the table file: " + path_.string());
        }
        //check if the offset is valid
        if(offset > file_size || HEADER_SIZE > file_size - offset){
            throw std::out_of_range("Row offset is out of range");
        }
        if(offset > static_cast<Row_offset>(std::numeric_limits<std::streamoff>::max())){
            throw std::out_of_range("Row offset is too large");
        }
        //move the pointer to the offset of the row
        fs.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
        if(!fs){
            throw std::runtime_error("Failed to seek to the row header: " + path_.string());
        }
        //read the header in file to array
        std::array<std::byte,HEADER_SIZE> header{};
        const std::streamsize header_size = static_cast<std::streamsize>(header.size());
        fs.read(reinterpret_cast<char*>(header.data()),header_size);
        if(!fs || fs.gcount() != header_size){
            throw std::runtime_error("Failed to read complete row header");
        }
        const std::uint8_t flag = std::to_integer<std::uint8_t>(header[0]);
        if(flag == DELETED){
            throw std::runtime_error("Row is already deleted");
        }
        if(flag != ACTIVE){
            throw std::runtime_error("Invalid row flag");
        }
        std::uint32_t payload_size = 0;
        for (std::uint32_t i = 0; i < 4; ++i) {
            const std::uint32_t current = std::to_integer<std::uint8_t>(header[1 + i]);
            payload_size |= current << (i * 8);
        }
        //check if the payload size is valid
        const std::uintmax_t payload_offset = static_cast<std::uintmax_t>(offset) + HEADER_SIZE;
        if(payload_size > file_size - payload_offset){
            throw std::runtime_error("cannot delete truncated row");
        }
        //write the first byte
        fs.seekp(static_cast<std::streamoff>(offset),std::ios::beg);
        if(!fs){
            throw std::runtime_error("Failed to seek to the row header for deletion: " + path_.string());   
        }
        const std::byte tomb = static_cast<std::byte>(DELETED);
        fs.write(reinterpret_cast<const char*>(&tomb),1);
        fs.flush();
        if(!fs){
            throw std::runtime_error("Failed to mark row as deleted: " + path_.string());
        }
    }
    std::vector<TableFile::row_entry> TableFile::scan_all_rows(){
        Row_offset offset = 0;
        std::vector<row_entry> res;
        std::error_code error;
        const std::uintmax_t file_size = std::filesystem::file_size(path_,error);
        if(error){
            throw std::runtime_error("Failed to get the size of the table file: " + path_.string());
        }
        while(offset < file_size){
            //check if the offset is valid
            if(offset > file_size || HEADER_SIZE > file_size - offset){
                throw std::runtime_error("Truncated row header");
            }
            if(offset > static_cast<Row_offset>(std::numeric_limits<std::streamoff>::max())){
                throw std::out_of_range("Row offset is too large");
            }
            //read the header in file to array
            fs.seekg(static_cast<std::streamoff>(offset),std::ios::beg);
            if(!fs){
                throw std::runtime_error("Failed to seek to the row header: " + path_.string());
            }
            std::array<std::byte,HEADER_SIZE> header{};
            const std::streamsize header_size = static_cast<std::streamsize>(header.size());
            fs.read(reinterpret_cast<char*>(header.data()),header_size);
            if(!fs || fs.gcount() != header_size){
                throw std::runtime_error("Failed to read complete row header");
            }
            const std::uint8_t flag = std::to_integer<std::uint8_t>(header[0]);
            if(flag != ACTIVE && flag != DELETED){
                throw std::runtime_error("Invalid row flag");
            }
            std::uint32_t payload_size = 0;
            for (std::uint32_t i = 0; i < 4; ++i) {
                const std::uint32_t current = std::to_integer<std::uint8_t>(header[1 + i]);
                payload_size |= current << (i * 8);
            }
            const std::uintmax_t payload_offset = static_cast<std::uintmax_t>(offset) + HEADER_SIZE;
            if(payload_size > file_size - payload_offset){
                throw std::runtime_error("Truncated row payload");
            }
            //read the row payload
            if(flag == ACTIVE){
                serialization::byte_vector payload(payload_size);
                const std::streamsize payload_size_stream = static_cast<std::streamsize>(payload.size());
                fs.read(reinterpret_cast<char*>(payload.data()),payload_size_stream);
                if(!fs || fs.gcount() != payload_size_stream){
                    throw std::runtime_error("Failed to read complete row payload");
                }
                Row row = serialization::deserialize_row(payload);
                res.push_back({offset,std::move(row)});
            }
            offset = static_cast<Row_offset>(payload_offset + payload_size);
        }
        return res;
    }
}