#include "storage/b_plus_tree.hpp"
#include <stdexcept>

namespace minidb{
    namespace {
        using Page = Pager::Page;
        void check_page_range(const Page& page,std::size_t offset,std::size_t byte_cnt){
            if(offset > page.size() || byte_cnt > page.size() - offset){
                throw std::out_of_range("B+ tree page access out of range");
            }
        }
        //read and write 1 byte in page begin with offset
        void write_u8(Page& page,std::size_t offset,std::uint8_t value){
            check_page_range(page,offset,1);
            page[offset] = static_cast<std::byte>(value);
        }
        std::uint8_t read_u8(const Page& page,std::size_t offset){
            check_page_range(page,offset,1);
            return std::to_integer<std::uint8_t>(page[offset]);
        }
        //read and write 2 bytes in page begin with offset
        void write_u16(Page& page,std::size_t offset,std::uint16_t value){
            check_page_range(page,offset,sizeof(value));
            for(std::size_t i = 0; i < sizeof(value); ++i){
                page[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFFu);
            }
        }
        std::uint16_t read_u16(const Page& page,std::size_t offset){
            check_page_range(page,offset,sizeof(std::uint16_t));
            std::uint16_t res = 0;
            for(std::size_t i = 0; i < sizeof(res); ++i){
                const std::uint16_t current = std::to_integer<std::uint16_t>(page[offset + i]);
                res |= (current << (i * 8));
            }
            return res;
        }
        //read and write 4 bytes in page begin with offset
        void write_u32(Page& page,std::size_t offset,std::uint32_t value){
            check_page_range(page,offset,sizeof(value));
            for(std::size_t i = 0; i < sizeof(value); ++i){
                page[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFFu);
            }
        }
        std::uint32_t read_u32(const Page& page,std::size_t offset){
            check_page_range(page,offset,sizeof(std::uint32_t));
            std::uint32_t res = 0;
            for(std::size_t i = 0; i < sizeof(res); ++i){
                const std::uint32_t current = std::to_integer<std::uint32_t>(page[offset + i]);
                res |= (current << (i * 8));
            }
            return res;
        }
        //read and write 8 bytes in page begin with offset
        void write_u64(Page& page,std::size_t offset,std::uint64_t value){
            check_page_range(page,offset,sizeof(value));
            for(std::size_t i = 0; i < sizeof(value); ++i){
                page[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFFu);
            }
        }
        std::uint64_t read_u64(const Page& page,std::size_t offset){
            check_page_range(page,offset,sizeof(std::uint64_t));
            std::uint64_t res = 0;
            for(std::size_t i = 0; i < sizeof(res); ++i){
                const std::uint64_t current = std::to_integer<std::uint64_t>(page[offset + i]);
                res |= (current << (i * 8));
            }
            return res;
        }
    }
}