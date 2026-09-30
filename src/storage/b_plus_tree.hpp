#pragma once
#include "core/data_type.hpp"
#include "core/value.hpp"
#include "storage/pager.hpp"
#include <cstddef>
#include <cstdint>
#include <array>
#include <filesystem>
#include <limits>

namespace minidb{
class BPlusTree{
public:
    using PageId = Pager::PageId;
    using row_offset = std::uint64_t;
    explicit BPlusTree(const std::filesystem::path& path,DataType key_type);
    BPlusTree(const BPlusTree& other) = delete;
    BPlusTree& operator=(const BPlusTree& other) = delete;
    [[nodiscard]] PageId get_root_page_id() const noexcept;
    [[nodiscard]] DataType get_keytype() const noexcept;
private:
    //sign of invalid page
    static constexpr PageId INVALID_PAGE_ID = std::numeric_limits<PageId>::max();
    //node header size = 32 bytes
    static constexpr std::size_t NODE_HEADER_SIZE = 32;
    //meta data of idx is page 0
    static constexpr PageId META_PAGE_ID = 0;
    //key slot size 
    static constexpr std::size_t KEY_SLOT_SIZE = sizeof(std::uint16_t) + Value::MAX_STRING_LENGTH;
    //entry size = key slot size + row offset or child page id size
    static constexpr std::size_t ENTRY_SIZE = KEY_SLOT_SIZE + sizeof(std::uint64_t);
    //max keys is 15
    static constexpr std::size_t MAX_KEYS = (Pager::PAGESIZE - NODE_HEADER_SIZE) / ENTRY_SIZE;
    static_assert(MAX_KEYS > 0,"B+ tree node must hold at least one key");
    enum class NodeType : std::uint8_t{
        LEAF = 1,
        INTERNAL = 2
    };
    enum class KeyTypeFlag : std::uint8_t{
        INT = 1,
        STRING = 2
    };
    struct NodeHeader{
        NodeType type = NodeType::LEAF;
        std::uint16_t key_cnt = 0;
        PageId parent_page_id = INVALID_PAGE_ID;
        //only used for leaf node
        PageId next_leaf_page_id = INVALID_PAGE_ID;
        //only used for internal node
        PageId first_child_page_id = INVALID_PAGE_ID;
    };
    Pager pager_;
    DataType key_type_;
    PageId root_page_id_ = INVALID_PAGE_ID;
    //create page 0 and empty root page
    void init_empty_tree();
    void load_metadata();
    void write_metadata();

    using KeySlot = std::array<std::byte, KEY_SLOT_SIZE>;
    //encode value to key slot
    [[nodiscard]] KeySlot encode_key(const Value& key) const;
    //decode key slot to value
    [[nodiscard]] Value decode_key(const KeySlot& slot) const;

    void write_node_header(Pager::Page& page,const NodeHeader& header) const;
    [[nodiscard]] NodeHeader read_node_header(const Pager::Page& page) const;
};
}