#include "storage/b_plus_tree.hpp"
#include <stdexcept>

namespace minidb{
    namespace {
        using Page = Pager::Page;
        //sign of meta data and node data
        constexpr std::uint32_t META_MAGIC = 0x5844494Du;
        constexpr std::uint32_t NODE_MAGIC = 0x45444F4Eu;
        constexpr std::uint32_t FORMAT_VERSION = 1;
        //meta data page offset
        constexpr std::size_t META_MAGIC_OFFSET = 0;
        constexpr std::size_t META_VERSION_OFFSET = 4;
        constexpr std::size_t META_KEY_TYPE_OFFSET = 8;
        constexpr std::size_t META_ROOT_PAGE_ID_OFFSET = 16;
        constexpr std::size_t META_PAGE_SIZE_OFFSET = 24;
        constexpr std::size_t META_KEY_SLOT_SIZE_OFFSET = 28;
        //node head offset
        constexpr std::size_t NODE_MAGIC_OFFSET = 0;
        constexpr std::size_t NODE_TYPE_OFFSET = 4;
        constexpr std::size_t NODE_RESERVED_OFFSET = 5;
        constexpr std::size_t NODE_KEY_CNT_OFFSET = 6;
        constexpr std::size_t NODE_PARENT_PAGE_ID_OFFSET = 8;
        constexpr std::size_t NODE_NEXT_LEAF_PAGE_ID_OFFSET = 16;
        constexpr std::size_t NODE_FIRST_CHILD_PAGE_ID_OFFSET = 24;
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
    BPlusTree::PageId BPlusTree::get_root_page_id() const noexcept{
        return root_page_id_;
    }
    DataType BPlusTree::get_keytype() const noexcept{
        return key_type_;
    }
    void BPlusTree::write_node_header(Page& page,const NodeHeader& header) const{
        if(header.key_cnt > MAX_KEYS){
            throw std::invalid_argument("B+ tree node key count exceeds maximum");
        }
        //check node type and corresponding page id
        switch(header.type){
            case NodeType::LEAF:
                if(header.first_child_page_id != INVALID_PAGE_ID){
                    throw std::invalid_argument("leaf node should not have first child page id");
                }
                break;
            case NodeType::INTERNAL:
                if(header.next_leaf_page_id != INVALID_PAGE_ID){
                    throw std::invalid_argument("internal node should not have next leaf page id");
                }
                break;
            default:
                throw std::invalid_argument("invalid node type");
        }
        //write node header to page
        write_u32(page,NODE_MAGIC_OFFSET,NODE_MAGIC);
        write_u8(page,NODE_TYPE_OFFSET,static_cast<std::uint8_t>(header.type));
        write_u16(page,NODE_RESERVED_OFFSET,0);
        write_u16(page,NODE_KEY_CNT_OFFSET,header.key_cnt);
        write_u64(page,NODE_PARENT_PAGE_ID_OFFSET,header.parent_page_id);
        write_u64(page,NODE_NEXT_LEAF_PAGE_ID_OFFSET,header.next_leaf_page_id);
        write_u64(page,NODE_FIRST_CHILD_PAGE_ID_OFFSET,header.first_child_page_id);
    }
}