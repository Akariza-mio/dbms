#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cstddef>

namespace minidb{
class Pager{
public:
    using PageId = std::uint64_t;
    static constexpr std::size_t PAGESIZE = 4096;
    using Page = std::array<std::byte, PAGESIZE>;

    explicit Pager(const std::filesystem::path& path);
    Pager(const Pager& other) = delete;
    Pager& operator=(const Pager& other) = delete;

    [[nodiscard]] PageId get_page_cnt() const noexcept;
    // read the page
    [[nodiscard]] Page read_page(PageId page_id);
    //cover the page 
    void write_page(PageId page_id,const Page& page);
    //allocate a new page
    [[nodiscard]] PageId allocate_page();
private:
    std::filesystem::path path_;
    std::fstream fs;
    PageId page_cnt = 0;
};
}