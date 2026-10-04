#include "storage/b_plus_tree.hpp"
#include <stdexcept>
#include <bit>
#include <string>
#include <utility>
#include <vector>

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
        constexpr std::size_t META_RESERVED_BEGIN = 9;
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
        return root_page_id;
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
                if(header.first_child_page_id == INVALID_PAGE_ID){
                    throw std::invalid_argument("internal node must have first child page id");
                }
                break;
            default:
                throw std::invalid_argument("invalid node type");
        }
        //write node header to page array
        write_u32(page,NODE_MAGIC_OFFSET,NODE_MAGIC);
        write_u8(page,NODE_TYPE_OFFSET,static_cast<std::uint8_t>(header.type));
        write_u8(page,NODE_RESERVED_OFFSET,0);
        write_u16(page,NODE_KEY_CNT_OFFSET,header.key_cnt);
        write_u64(page,NODE_PARENT_PAGE_ID_OFFSET,header.parent_page_id);
        write_u64(page,NODE_NEXT_LEAF_PAGE_ID_OFFSET,header.next_leaf_page_id);
        write_u64(page,NODE_FIRST_CHILD_PAGE_ID_OFFSET,header.first_child_page_id);
    }
    BPlusTree::NodeHeader BPlusTree::read_node_header(const Page& page) const{
        //check header is valid
        if(read_u32(page, NODE_MAGIC_OFFSET) != NODE_MAGIC){
            throw std::runtime_error("invalid B+ tree node magic");
        }
        if(read_u8(page,NODE_RESERVED_OFFSET) !=0 ){
            throw std::runtime_error("invalid B+ tree node reserved byte");
        }
        //get type
        const std::uint8_t type_value = read_u8(page,NODE_TYPE_OFFSET);
        NodeType type;
        switch(type_value){
            case 1:
                type = NodeType::LEAF;
                break;
            case 2:
                type = NodeType::INTERNAL;
                break;
            default:
                throw std::runtime_error("invalid B+ tree node type");
        }
        const std::uint16_t key_cnt = read_u16(page,NODE_KEY_CNT_OFFSET);
        if(key_cnt > MAX_KEYS){
            throw std::runtime_error("invalid B+ tree node key count");
        }
        //construct node header and check
        NodeHeader header;
        header.type = type;
        header.key_cnt = key_cnt;
        header.parent_page_id = read_u64(page,NODE_PARENT_PAGE_ID_OFFSET);
        header.next_leaf_page_id = read_u64(page,NODE_NEXT_LEAF_PAGE_ID_OFFSET);
        header.first_child_page_id = read_u64(page,NODE_FIRST_CHILD_PAGE_ID_OFFSET);
        if(header.type == NodeType::LEAF){
            if(header.first_child_page_id != INVALID_PAGE_ID){
                throw std::runtime_error("leaf node should not have first child page id");
            }
        }else{
            if(header.next_leaf_page_id != INVALID_PAGE_ID){
                throw std::runtime_error("internal node should not have next leaf page id");
            }
            if(header.first_child_page_id == INVALID_PAGE_ID){
                throw std::runtime_error("internal node should have first child page id");
            }
        }
        return header;
    }
    BPlusTree::KeySlot BPlusTree::encode_key(const Value& key) const{
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        KeySlot slot{};
        //construct key slot based on key type
        switch(key_type_){
            case DataType::INT:{
                const std::uint32_t bits = std::bit_cast<std::uint32_t>(key.as_int());
                for(std::size_t i = 0; i < sizeof(bits); ++i){
                    slot[i] = static_cast<std::byte>((bits >> (i * 8)) & 0xFFu);
                }
                return slot;
            }
            case DataType::STRING:{
                const std::string& value = key.as_string();
                if(value.size() > Value::MAX_STRING_LENGTH){
                    throw std::invalid_argument("string key exceeds maximum length");
                }
                const std::uint16_t len = static_cast<std::uint16_t>(value.size());
                slot[0] = static_cast<std::byte>(len & 0xFFu);
                slot[1] = static_cast<std::byte>((len >> 8) & 0xFFu);
                for(std::size_t i = 0; i < value.size(); ++i){
                    const unsigned char c = static_cast<unsigned char>(value[i]);
                    slot[2 + i] = static_cast<std::byte>(c);
                }
                return slot;
            }
            //check invalid key type
            case DataType::FLOAT:{
                throw std::invalid_argument("float key type is not supported in B+ tree");
            }
            default:
                throw std::invalid_argument("unsupported key type in B+ tree");
        }
    }
    Value BPlusTree::decode_key(const KeySlot& slot) const{
        //construct value based on key type
        switch(key_type_){
            case DataType::INT:{
                std::uint32_t bits = 0;
                for(std::size_t i = 0; i < sizeof(bits); ++i){
                    const std::uint32_t current = std::to_integer<std::uint8_t>(slot[i]);
                    bits |= (current << (i * 8));
                }
                for(std::size_t i = sizeof(bits); i < slot.size(); ++i){
                    if(slot[i] != std::byte{0}){
                        throw std::runtime_error("invalid INT key padding");
                    }
                }
                const std::int32_t value = std::bit_cast<std::int32_t>(bits);
                return Value(value);
            }
            case DataType::STRING:{
                const std::uint16_t low = std::to_integer<std::uint8_t>(slot[0]);
                const std::uint16_t high = std::to_integer<std::uint8_t>(slot[1]);
                const std::uint16_t len = (high << 8) | low;
                if(len > Value::MAX_STRING_LENGTH){
                    throw std::runtime_error("invalid string key length");
                }
                std::string value;
                value.reserve(len);
                for(std::size_t i = 0; i < len; ++i){
                    const unsigned char c = std::to_integer<unsigned char>(slot[2 + i]);
                    value.push_back(static_cast<char>(c));
                }
                for(std::size_t i = 2 + len; i < slot.size(); ++i){
                    if(slot[i] != std::byte{0}){
                        throw std::runtime_error("invalid string key padding");
                    }
                }
                return Value(std::move(value));
            }
            case DataType::FLOAT:{
                throw std::invalid_argument("float key type is not supported in B+ tree");
            }
            default:
                throw std::invalid_argument("unsupported key type in B+ tree");
        }
    }
    void BPlusTree::write_metadata(){
        //check root page id and key type
        if(pager_.get_page_cnt() == 0){
            throw std::logic_error("B+ tree metadata page is not allocated");
        }
        if(root_page_id == INVALID_PAGE_ID || root_page_id == META_PAGE_ID || root_page_id >= pager_.get_page_cnt()){
            throw std::logic_error("B+ tree root page id is invalid");
        }
        std::uint8_t key_type_tag = 0;
        switch(key_type_){
            case DataType::INT:
                key_type_tag = static_cast<uint8_t>(KeyTypeFlag::INT);
                break;
            case DataType::STRING:
                key_type_tag = static_cast<uint8_t>(KeyTypeFlag::STRING);
                break;
            case DataType::FLOAT:
                throw std::logic_error("float key type is not supported in B+ tree");
            default:
                throw std::logic_error("unsupported key type in B+ tree");
        }
        Page metadata{};
        //rebuild and overwrite metadata page
        write_u32(metadata,META_MAGIC_OFFSET,META_MAGIC);
        write_u32(metadata,META_VERSION_OFFSET,FORMAT_VERSION);
        write_u8(metadata,META_KEY_TYPE_OFFSET,key_type_tag);
        write_u64(metadata,META_ROOT_PAGE_ID_OFFSET,root_page_id);
        write_u32(metadata,META_PAGE_SIZE_OFFSET,static_cast<std::uint32_t>(Pager::PAGESIZE));
        write_u32(metadata,META_KEY_SLOT_SIZE_OFFSET,static_cast<std::uint32_t>(KEY_SLOT_SIZE));
        pager_.write_page(META_PAGE_ID,metadata);
    }
    void BPlusTree::load_metadata(){
        const PageId page_cnt = pager_.get_page_cnt();
        if(page_cnt < 2){
            throw std::runtime_error("B+ tree index file does not contain metadata and root page");
        }
        //read metadata page
        const Page metadata = pager_.read_page(META_PAGE_ID);
        //several checks
        if(read_u32(metadata,META_MAGIC_OFFSET) != META_MAGIC){
            throw std::runtime_error("invalid B+ tree metadata magic");
        }
        if(read_u32(metadata,META_VERSION_OFFSET) != FORMAT_VERSION){
            throw std::runtime_error("unsupported B+ tree metadata version");
        }
        for(std::size_t i = META_RESERVED_BEGIN; i < META_ROOT_PAGE_ID_OFFSET; ++i){
            if(metadata[i] != std::byte{0}){
                throw std::runtime_error("invalid B+ tree metadata reserved byte");
            }
        }
        if(read_u32(metadata,META_PAGE_SIZE_OFFSET) != static_cast<std::uint32_t>(Pager::PAGESIZE)){
            throw std::runtime_error("B+ tree metadata page size does not match");
        }
        if(read_u32(metadata,META_KEY_SLOT_SIZE_OFFSET) != static_cast<std::uint32_t>(KEY_SLOT_SIZE)){
            throw std::runtime_error("B+ tree metadata key slot size does not match");
        }
        //get key type
        const std::uint8_t key_type_tag = read_u8(metadata,META_KEY_TYPE_OFFSET);
        DataType type_value;
        switch(key_type_tag){
            case 1:
                type_value = DataType::INT;
                break;
            case 2:
                type_value = DataType::STRING;
                break;
            default:
                throw std::runtime_error("invalid B+ tree key type");
        }
        if(type_value != key_type_){
            throw std::runtime_error("B+ tree key type does not match");
        }
        //get root page id and check
        const PageId new_root_page_id = read_u64(metadata,META_ROOT_PAGE_ID_OFFSET);
        if(new_root_page_id == INVALID_PAGE_ID || new_root_page_id == META_PAGE_ID || new_root_page_id >= page_cnt){
            throw std::runtime_error("B+ tree root page id is invalid");
        }
        //get root page and check
        const Page root_page = pager_.read_page(new_root_page_id);
        const NodeHeader root_header = read_node_header(root_page);
        if(root_header.parent_page_id != INVALID_PAGE_ID){
            throw std::runtime_error("root node should not have parent page id");
        }
        if(root_header.type == NodeType::LEAF){
            if(root_header.next_leaf_page_id != INVALID_PAGE_ID){
                throw std::runtime_error("root leaf node should not have next leaf page id");
            }
        }else{
            if(root_header.key_cnt == 0){
                throw std::runtime_error("internal node should have at least one key");
            }
            if(root_header.first_child_page_id == INVALID_PAGE_ID || root_header.first_child_page_id == META_PAGE_ID || root_header.first_child_page_id >= page_cnt){
                throw std::runtime_error("internal node have valid first child page id");
            }
        }
        root_page_id = new_root_page_id;
    }
    void BPlusTree::init_empty_tree(){
        if(pager_.get_page_cnt() != 0){
            throw std::logic_error("B+ tree index file is not empty, cannot initialize empty tree");
        }
        //allocate metadata page and root page
        const PageId meta_page_id = pager_.allocate_page();
        if(meta_page_id != META_PAGE_ID){
            throw std::logic_error("B+ tree metadata page id is not 0");
        }
        const PageId new_root_page_id = pager_.allocate_page();
        if(new_root_page_id != 1){
            throw std::logic_error("first B+ tree root page id must be 1");
        }
        //writr root first and then write metadata
        Page root_page{};
        NodeHeader root_header{NodeType::LEAF,0,INVALID_PAGE_ID,INVALID_PAGE_ID,INVALID_PAGE_ID};
        write_node_header(root_page,root_header);
        pager_.write_page(new_root_page_id,root_page);
        root_page_id = new_root_page_id;
        write_metadata();
    }
    //constructor
    BPlusTree::BPlusTree(const std::filesystem::path& path,DataType key_type)
        :pager_(path),key_type_(key_type){
            switch(key_type_){
                case DataType::INT:
                case DataType::STRING:
                    break;
                case DataType::FLOAT:
                    throw std::invalid_argument("float key type is not supported in B+ tree");
                default:
                    throw std::invalid_argument("unsupported key type in B+ tree");
            }
            if(pager_.get_page_cnt() == 0){
                init_empty_tree();
            }else{
                load_metadata();    
            }
        }
    std::size_t BPlusTree::entry_offset(std::size_t index) const{
        if(index >= MAX_KEYS){
            throw std::out_of_range("B+ tree entry index out of range");
        }
        return NODE_HEADER_SIZE + index * ENTRY_SIZE;
    }
    BPlusTree::KeySlot BPlusTree::read_key_slot(const Pager::Page& page,std::size_t index) const{
        const std::size_t offset = entry_offset(index);
        check_page_range(page,offset,KEY_SLOT_SIZE);
        KeySlot slot{};
        //copy bytes from page to key slot
        for(std::size_t i = 0; i < KEY_SLOT_SIZE; ++i){
            slot[i] = page[offset + i];
        }
        return slot;
    }
    void BPlusTree::write_key_slot(Pager::Page& page,std::size_t index,const KeySlot& slot) const{
        const std::size_t offset = entry_offset(index);
        check_page_range(page,offset,KEY_SLOT_SIZE);
        //copy bytes from key slot to page
        for(std::size_t i = 0; i < KEY_SLOT_SIZE; ++i){
            page[offset + i] = slot[i];
        }
    }
    BPlusTree::row_offset BPlusTree::read_leaf_row_offset(const Pager::Page& page,std::size_t index) const{
        const std::size_t offset = entry_offset(index) + KEY_SLOT_SIZE;
        return read_u64(page,offset);
    }
    void BPlusTree::write_leaf_entry(Pager::Page& page,std::size_t index,const Value& key,row_offset offset) const{
        const KeySlot slot = encode_key(key);
        write_key_slot(page,index,slot);
        write_u64(page,entry_offset(index) + KEY_SLOT_SIZE,offset);
    }
    std::size_t BPlusTree::find_leaf_position(const Pager::Page& page,std::uint16_t key_cnt,const Value& key) const{
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        if(key_cnt > MAX_KEYS){
            throw std::invalid_argument("key count exceeds maximum in B+ tree node");
        }
        std::size_t l = 0, r = key_cnt;
        while(l < r){
            const std::size_t mid = (l + r) / 2;
            const Value mid_key = decode_key(read_key_slot(page,mid));
            if(mid_key < key){
                l = mid + 1;
            }else{
                r = mid;
            }
        }
        //return the first position that the key is >= the given key
        return l;
    }
    std::optional<BPlusTree::row_offset> BPlusTree::search(const Value& key){
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        //find the leaf contain the key
        const PageId leaf_page_id = find_leaf_page_id(key);
        const Page leaf_page = pager_.read_page(leaf_page_id);
        const NodeHeader leaf_header = read_node_header(leaf_page);
        if(leaf_header.type != NodeType::LEAF) {
            throw std::logic_error("should return a leaf node");
        }
        const std::size_t pos = find_leaf_position(leaf_page,leaf_header.key_cnt,key);
        if(pos >= leaf_header.key_cnt){
            return std::nullopt;
        }
        const Value stored_key = decode_key(read_key_slot(leaf_page,pos));
        if(stored_key != key){
            return std::nullopt;
        }
        return read_leaf_row_offset(leaf_page,pos);
    }
    void BPlusTree::insert(const Value& key,row_offset offset){
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        const PageId leaf_page_id = find_leaf_page_id(key);
        Page leaf_page = pager_.read_page(leaf_page_id);
        NodeHeader leaf_header = read_node_header(leaf_page);
        if(leaf_header.type != NodeType::LEAF){
            throw std::logic_error("should return a leaf node");
        }
        const std::size_t pos = find_leaf_position(leaf_page,leaf_header.key_cnt,key);
        if(pos < leaf_header.key_cnt){
            const Value stored_key = decode_key(read_key_slot(leaf_page,pos));
            if(stored_key == key){
                throw std::invalid_argument("duplicate key insertion");
            }
        }
        if(leaf_header.key_cnt >= MAX_KEYS){
            //insert new key into existing parent
            if(leaf_header.parent_page_id != INVALID_PAGE_ID){
                const Page parent_page = pager_.read_page(leaf_header.parent_page_id);
                const NodeHeader parent_header = read_node_header(parent_page);
                if(parent_header.type != NodeType::INTERNAL){
                    throw std::logic_error("parent node should be internal node");
                }
            }
            const LeafSplitResult res = split_leaf_and_insert(
                leaf_page_id,
                leaf_page,
                leaf_header,
                key,
                offset,
                pos
            );
            insert_into_parent(leaf_page_id,res.split_key,res.right_page_id);
            return;
        }
        //move entries after pos to the right to make space for new entry
        for(std::size_t destination = leaf_header.key_cnt; destination > pos; --destination){
            const std::size_t source_offset = entry_offset(destination - 1);
            const std::size_t destination_offset = entry_offset(destination);
            for(std::size_t i = 0; i < ENTRY_SIZE; ++i){
                leaf_page[destination_offset + i] = leaf_page[source_offset + i];
            }
        }
        //add new entry at pos
        write_leaf_entry(leaf_page,pos,key,offset);
        ++leaf_header.key_cnt;
        write_node_header(leaf_page,leaf_header);
        pager_.write_page(leaf_page_id,leaf_page);
    }
    [[nodiscard]] BPlusTree::PageId BPlusTree::read_internal_child_page_id(const Pager::Page& page,std::size_t index) const{
        return read_u64(page,entry_offset(index) + KEY_SLOT_SIZE);
    }
    void BPlusTree::write_internal_entry(Pager::Page& page,std::size_t index,const Value& key,PageId right_child_page_id) const{
        write_key_slot(page,index,encode_key(key));
        write_u64(page,entry_offset(index) + KEY_SLOT_SIZE,right_child_page_id);
    }
    BPlusTree::PageId BPlusTree::find_leaf_page_id(const Value& key){
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        PageId now_page_id = root_page_id;
        const PageId page_cnt = pager_.get_page_cnt();
        //search page_cnt pages at most
        for(PageId visited = 0; visited < page_cnt; ++visited){
            const Page page = pager_.read_page(now_page_id);
            const NodeHeader header = read_node_header(page);
            if(header.type == NodeType::LEAF){
                return now_page_id;
            }
            //not leaf node,binary search the first key that > the given key
            std::size_t l = 0, r = header.key_cnt;
            while(l < r){
                const std::size_t mid = (l + r) / 2;
                const Value mid_key = decode_key(read_key_slot(page,mid));
                if(key < mid_key){
                    r = mid;
                }else{
                    l = mid + 1;
                }
            }
            PageId child_page_id;
            if(l == 0){
                child_page_id = header.first_child_page_id;
            }else{
                child_page_id = read_internal_child_page_id(page,l - 1);
            }
            if(child_page_id == INVALID_PAGE_ID || child_page_id == META_PAGE_ID || child_page_id >= page_cnt){
                throw std::runtime_error("invalid child page id");
            }
            now_page_id = child_page_id;
        }
        throw std::runtime_error("B+ tree search exceeded maximum depth, possible cycle in tree");
    }
    BPlusTree::LeafSplitResult BPlusTree::split_leaf_and_insert(
        PageId leaf_page_id,
        const Pager::Page& leaf_page,
        const NodeHeader& leaf_header,
        const Value& key,
        row_offset offset,
        std::size_t insert_pos
    ){
        if(leaf_header.type != NodeType::LEAF){
            throw std::invalid_argument("cannot split a non_leaf node");
        }
        if(leaf_header.key_cnt != MAX_KEYS){
            throw std::invalid_argument("cannot split a non_full node");
        }
        if(insert_pos > leaf_header.key_cnt){
            throw std::invalid_argument("insert position is out of range");
        }
        //define a entry pair for vector holding
        using Entry = std::pair<Value,row_offset>;
        std::vector<Entry> entries;
        entries.reserve(MAX_KEYS + 1);
        //copy entries to vector and insert the new entry
        for(std::size_t i = 0; i < leaf_header.key_cnt; ++i){
            entries.emplace_back(decode_key(read_key_slot(leaf_page,i)),read_leaf_row_offset(leaf_page,i));
        }
        entries.insert(entries.begin() + static_cast<std::vector<Entry>::difference_type>(insert_pos),Entry{key,offset});
        const std::size_t left_cnt = entries.size() / 2;
        const std::size_t right_cnt = entries.size() - left_cnt;
        //create new right leaf page
        const PageId right_page_id = pager_.allocate_page();
        Pager::Page new_left_page{};
        Pager::Page new_right_page{};
        //construct header of new left page
        NodeHeader new_left_header = leaf_header;
        new_left_header.key_cnt = static_cast<std::uint16_t>(left_cnt);
        //construct header of new right page
        new_left_header.next_leaf_page_id = right_page_id;
        NodeHeader new_right_header{
            NodeType::LEAF,
            static_cast<std::uint16_t>(right_cnt),
            leaf_header.parent_page_id,
            leaf_header.next_leaf_page_id,
            INVALID_PAGE_ID
        };
        //construct new left page
        for(std::size_t i = 0; i < left_cnt; ++i){
            write_leaf_entry(new_left_page,i,entries[i].first,entries[i].second);
        }
        //construct new right page
        for(std::size_t i = 0; i < right_cnt; ++i){
            write_leaf_entry(new_right_page,i,entries[left_cnt + i].first,entries[left_cnt + i].second);
        }
        //write: node header to page,page to file
        write_node_header(new_left_page,new_left_header);
        write_node_header(new_right_page,new_right_header);
        pager_.write_page(leaf_page_id,new_left_page);
        pager_.write_page(right_page_id,new_right_page);
        return LeafSplitResult{entries[left_cnt].first,right_page_id};
    }
    void BPlusTree::insert_into_parent(PageId left_page_id,const Value& separator_key,PageId right_page_id){
        Pager::Page left_page = pager_.read_page(left_page_id);
        Pager::Page right_page = pager_.read_page(right_page_id);
        NodeHeader left_header = read_node_header(left_page);
        NodeHeader right_header = read_node_header(right_page);
        if(left_header.parent_page_id != right_header.parent_page_id){
            throw std::runtime_error("left and right page should have the same parent"); 
        }
        //if left page has no parent, create a new root page
        if(left_header.parent_page_id == INVALID_PAGE_ID){
            if(left_page_id != root_page_id){
                throw std::runtime_error("only root page can have no parent");
            }
            //construct new root page and write to file
            const PageId new_root_page_id = pager_.allocate_page();
            Pager::Page new_root_page{};
            NodeHeader new_root_header{
                NodeType::INTERNAL,
                1,
                INVALID_PAGE_ID,
                INVALID_PAGE_ID,
                left_page_id
            };
            write_internal_entry(new_root_page,0,separator_key,right_page_id);
            write_node_header(new_root_page,new_root_header);
            left_header.parent_page_id = new_root_page_id;
            right_header.parent_page_id = new_root_page_id;
            write_node_header(left_page,left_header);
            write_node_header(right_page,right_header);
            pager_.write_page(left_page_id,left_page);
            pager_.write_page(right_page_id,right_page);
            pager_.write_page(new_root_page_id,new_root_page);
            root_page_id = new_root_page_id;
            //renew metadata page
            write_metadata();
            return;
        }
        //nodes has parent
        const PageId parent_page_id = left_header.parent_page_id;
        Pager::Page parent_page = pager_.read_page(parent_page_id);
        NodeHeader parent_header = read_node_header(parent_page);
        if(parent_header.type != NodeType::INTERNAL){
            throw std::logic_error("parent page should be an internal node");
        }
        //insert key to parent page
        std::size_t insert_pos = 0;
        bool left_child_found = false;
        if(parent_header.first_child_page_id == left_page_id){
            left_child_found = true;
            insert_pos = 0;
        }else{
            //if child i is splitting, then the separator key is inserted at position i + 1
            for(std::size_t i = 0; i < parent_header.key_cnt; ++i){
                if(read_internal_child_page_id(parent_page,i) == left_page_id){
                    left_child_found = true;
                    insert_pos = i + 1;
                    break;
                }
            }
        }
        if(!left_child_found){
            throw std::logic_error("left page is not a child of its parent");
        }
        //parent is full
        if(parent_header.key_cnt >= MAX_KEYS){
            const InternalSplitResult res = split_internal_and_insert(
                parent_page_id,
                parent_page,
                parent_header,
                separator_key,
                right_page_id,
                insert_pos
            );
            insert_into_parent(parent_page_id,res.promoted_key,res.right_page_id);
            return;
        }
        //move right entries to make a space for new entry
        for(std::size_t destination = parent_header.key_cnt; destination > insert_pos; --destination){
            const std::size_t source_offset = entry_offset(destination - 1);
            const std::size_t destination_offset = entry_offset(destination);
            for(std::size_t i = 0; i < ENTRY_SIZE; ++i){
                parent_page[destination_offset + i] = parent_page[source_offset + i];
            }
        }
        write_internal_entry(parent_page,insert_pos,separator_key,right_page_id);
        ++parent_header.key_cnt;
        write_node_header(parent_page,parent_header);
        pager_.write_page(parent_page_id,parent_page);
    }
    //update the parent of the right child internal page
    void BPlusTree::update_child_parent(PageId child_page_id,PageId new_parent_page_id){
        const PageId page_cnt = pager_.get_page_cnt();
        if(child_page_id == INVALID_PAGE_ID || child_page_id == META_PAGE_ID || child_page_id >= page_cnt){
            throw std::invalid_argument("invalid child page id");
        }
        if(new_parent_page_id == INVALID_PAGE_ID || new_parent_page_id == META_PAGE_ID || new_parent_page_id >= page_cnt){
            throw std::invalid_argument("invalid new parent page id");
        }
        Pager::Page child_page = pager_.read_page(child_page_id);
        NodeHeader child_header = read_node_header(child_page);
        child_header.parent_page_id = new_parent_page_id;
        write_node_header(child_page,child_header);
        pager_.write_page(child_page_id,child_page);
    }
    BPlusTree::InternalSplitResult BPlusTree::split_internal_and_insert(
        PageId internal_page_id,
        const Pager::Page& internal_page,
        const NodeHeader& internal_header,
        const Value& separator_key,
        PageId right_child_page_id,
        std::size_t insert_pos
    ){
        if(internal_header.type != NodeType::INTERNAL){
            throw std::invalid_argument("cannot split a non_internal node");
        }
        if(internal_header.key_cnt != MAX_KEYS){
            throw std::logic_error("cannot split a non_full node");
        }
        if(insert_pos > internal_header.key_cnt){
            throw std::out_of_range("insert position is out of range");
        }
        if(separator_key.get_type() != key_type_){
            throw std::invalid_argument("separator key type does not match B+ tree key type");
        }
        using KeyList = std::vector<Value>;
        using ChildList = std::vector<PageId>;
        KeyList keys;
        ChildList children;
        keys.reserve(MAX_KEYS + 1);
        children.reserve(MAX_KEYS + 2);
        //copy existing keys and children to vectors
        children.push_back(internal_header.first_child_page_id);
        for(std::size_t i = 0; i < internal_header.key_cnt; ++i){
            keys.push_back(decode_key(read_key_slot(internal_page,i)));
            children.push_back(read_internal_child_page_id(internal_page,i));
        }
        //insert new separator key
        keys.insert(keys.begin() + static_cast<KeyList::difference_type>(insert_pos),separator_key);
        children.insert(children.begin() + static_cast<ChildList::difference_type>(insert_pos + 1),right_child_page_id);
        if(keys.size() != MAX_KEYS + 1 || children.size() != MAX_KEYS + 2){
            throw std::logic_error("internal node split vectors size mismatch");
        }
        const std::size_t promoted_index = keys.size() / 2;
        const Value promoted_key = keys[promoted_index];
        const std::size_t left_cnt = promoted_index;
        const std::size_t right_cnt = keys.size() - promoted_index - 1;
        const PageId new_right_page_id = pager_.allocate_page();
        Pager::Page new_left_page{};
        Pager::Page new_right_page{};
        //construct new left page header:key 0-7 children 0-8
        NodeHeader new_left_header{
            NodeType::INTERNAL,
            static_cast<std::uint16_t>(left_cnt),
            internal_header.parent_page_id,
            INVALID_PAGE_ID,
            children[0]
        };
        for(std::size_t i = 0; i < left_cnt; ++i){
            write_internal_entry(new_left_page,i,keys[i],children[i + 1]);
        }
        //construct new right page header:key 9-15 children 9-16
        NodeHeader new_right_header{
            NodeType::INTERNAL,
            static_cast<std::uint16_t>(right_cnt),
            internal_header.parent_page_id,
            INVALID_PAGE_ID,
            children[promoted_index + 1]
        };
        for(std::size_t i = 0; i < right_cnt; ++i){
            write_internal_entry(new_right_page,i,keys[promoted_index + 1 + i],children[promoted_index + 2 + i]);
        }
        write_node_header(new_left_page,new_left_header);
        write_node_header(new_right_page,new_right_header);
        pager_.write_page(internal_page_id,new_left_page);
        pager_.write_page(new_right_page_id,new_right_page);
        //update new right internal node's children's parent page id
        for(std::size_t i = promoted_index + 1; i < children.size(); ++i){
            update_child_parent(children[i],new_right_page_id);
        }
        return InternalSplitResult{promoted_key,new_right_page_id};
    }
    bool BPlusTree::erase(const Value& key){
        if(key.get_type() != key_type_){
            throw std::invalid_argument("key type does not match B+ tree key type");
        }
        //get the deleting leaf page and its header
        const PageId leaf_page_id = find_leaf_page_id(key);
        Pager::Page leaf_page = pager_.read_page(leaf_page_id);
        NodeHeader leaf_header = read_node_header(leaf_page);
        if(leaf_header.type != NodeType::LEAF){
            throw std::logic_error("should return a leaf node");
        }
        const std::size_t pos = find_leaf_position(leaf_page,leaf_header.key_cnt,key);
        if(pos >= leaf_header.key_cnt){
            return false;
        }
        const Value stored_key = decode_key(read_key_slot(leaf_page,pos));
        if(stored_key != key){
            return false;
        }
        //move entries to left to overwrite the deleted entry
        for(std::size_t source = pos + 1; source < leaf_header.key_cnt; ++source){
            const std::size_t source_offset = entry_offset(source);
            const std::size_t destination_offset = entry_offset(source - 1);
            for(std::size_t i = 0; i < ENTRY_SIZE; ++i){
                leaf_page[destination_offset + i] = leaf_page[source_offset + i];
            }
        }
        //clear the last entry
        const std::size_t last_offset = entry_offset(leaf_header.key_cnt - 1);
        for(std::size_t i = 0; i < ENTRY_SIZE; ++i){
            leaf_page[last_offset + i] = std::byte{0};
        }
        --leaf_header.key_cnt;
        write_node_header(leaf_page,leaf_header);
        pager_.write_page(leaf_page_id,leaf_page);
        return true;
    }
}