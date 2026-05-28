#pragma once
#include <fstream>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include "common/Vector.h"

namespace dbms {

constexpr size_t PAGE_SIZE = 4096;

class Pager {
private:
    std::fstream file_;
    std::string filename_;
    uint32_t num_pages_;

public:
    Pager(const std::string& filename) : filename_(filename) {
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        if (!file_.is_open()) {
            file_.open(filename, std::ios::out | std::ios::binary);
            file_.close();
            file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
        
        file_.seekg(0, std::ios::end);
        size_t file_size = file_.tellg();
        num_pages_ = file_size / PAGE_SIZE;
        file_.clear();
    }

    ~Pager() {
        if (file_.is_open()) {
            file_.close();
        }
    }

    void read_page(uint32_t page_id, void* dest) {
        if (page_id >= num_pages_) {
            throw std::out_of_range("Page ID out of range");
        }
        file_.seekg(page_id * PAGE_SIZE, std::ios::beg);
        file_.read(static_cast<char*>(dest), PAGE_SIZE);
    }

    void write_page(uint32_t page_id, const void* src) {
        if (page_id > num_pages_) {
            throw std::out_of_range("Writing past allowed page limit");
        }
        if (page_id == num_pages_) {
            num_pages_++;
        }
        file_.seekp(page_id * PAGE_SIZE, std::ios::beg);
        file_.write(static_cast<const char*>(src), PAGE_SIZE);
        file_.flush();
    }

    uint32_t allocate_page() {
        uint32_t new_page_id = num_pages_;
        char empty_page[PAGE_SIZE] = {0};
        write_page(new_page_id, empty_page);
        return new_page_id;
    }

    uint32_t get_num_pages() const {
        return num_pages_;
    }
    
    void clear_file() {
        file_.close();
        file_.open(filename_, std::ios::out | std::ios::trunc | std::ios::binary);
        file_.close();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
        num_pages_ = 0;
    }
};

} // namespace dbms
