#pragma once
#include "core/row.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace minidb{
class TableFile{
public:
    //the offset of one row record in .dat
    using Row_offset = std::uint64_t;
    explicit TableFile(const std::filesystem::path& path);
    TableFile(const TableFile& other) = delete;
    TableFile& operator=(const TableFile& other) = delete;
    //write row to the table file as bytes
    [[nodiscard]] Row_offset insert_row(const Row& row);
    [[nodiscard]] Row get_row(Row_offset offset);
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