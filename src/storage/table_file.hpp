#pragma once
#include "core/row.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace minidb{
class TableFile{
public:
    //the offset of one row record in .dat
    using Row_offset = std::uint64_t;
    //scan result class
    struct row_entry{
        Row_offset offset;
        Row row;
    };
    explicit TableFile(const std::filesystem::path& path);
    TableFile(const TableFile& other) = delete;
    TableFile& operator=(const TableFile& other) = delete;
    //write row to the table file as bytes
    [[nodiscard]] Row_offset insert_row(const Row& row);
    [[nodiscard]] Row get_row(Row_offset offset);
    //mark a row as deleted without removing its bytes
    void delete_row(Row_offset offset);
    [[nodiscard]] std::vector<row_entry> scan_all_rows();
private:
    //signal of the row record, 0 for active, 1 for deleted
    static constexpr std::uint8_t ACTIVE = 0;
    static constexpr std::uint8_t DELETED = 1;
    //header = signal + bytes of the row (4 bytes)
    static constexpr std::size_t HEADER_SIZE = 5;
    std::filesystem::path path_;
    std::fstream fs;
};
}