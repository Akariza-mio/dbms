#pragma once
#include "Pager.h"
#include "Serializer.h"
#include <iostream>
#include <stdexcept>

namespace dbms {

constexpr size_t BPLUS_HEADER_SIZE = 7;
constexpr size_t BPLUS_ENTRY_SIZE = 263; // 259 (Key) + 4 (Value/PageID)
constexpr size_t BPLUS_MAX_KEYS = (PAGE_SIZE - BPLUS_HEADER_SIZE - 4) / BPLUS_ENTRY_SIZE;

class BPlusTree {
private:
    Pager* pager_;
    uint32_t root_page_id_;

    struct NodeHeader {
        bool is_leaf;
        uint16_t num_keys;
        uint32_t next_leaf;
    };

    void read_header(const char* page, NodeHeader& header) const {
        header.is_leaf = page[0] != 0;
        std::memcpy(&header.num_keys, page + 1, 2);
        std::memcpy(&header.next_leaf, page + 3, 4);
    }

    void write_header(char* page, const NodeHeader& header) {
        page[0] = header.is_leaf ? 1 : 0;
        std::memcpy(page + 1, &header.num_keys, 2);
        std::memcpy(page + 3, &header.next_leaf, 4);
    }

    void insert_into_internal(Vector<uint32_t>& path, const Value& key, uint32_t child_page_id) {
        if (path.empty()) {
            // Create new root
            uint32_t new_root_id = pager_->allocate_page();
            char root_page[PAGE_SIZE] = {0};
            NodeHeader root_header = {false, 1, 0};
            write_header(root_page, root_header);
            
            // Write left child (old root)
            std::memcpy(root_page + BPLUS_HEADER_SIZE, &root_page_id_, 4);
            // Write key
            Serializer::serialize_value_fixed(key, root_page + BPLUS_HEADER_SIZE + 4);
            // Write right child
            std::memcpy(root_page + BPLUS_HEADER_SIZE + 4 + BPLUS_KEY_SIZE, &child_page_id, 4);
            
            pager_->write_page(new_root_id, root_page);
            root_page_id_ = new_root_id;
            return;
        }

        uint32_t parent_id = path[path.size() - 1];
        path.pop_back();

        char page[PAGE_SIZE];
        pager_->read_page(parent_id, page);
        NodeHeader header;
        read_header(page, header);

        if (header.num_keys < BPLUS_MAX_KEYS) {
            // Insert without split
            uint16_t pos = 0;
            while (pos < header.num_keys) {
                Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + 4 + pos * BPLUS_ENTRY_SIZE);
                if (key < k) break;
                pos++;
            }
            
            // Shift right
            if (pos < header.num_keys) {
                std::memmove(
                    page + BPLUS_HEADER_SIZE + 4 + (pos + 1) * BPLUS_ENTRY_SIZE,
                    page + BPLUS_HEADER_SIZE + 4 + pos * BPLUS_ENTRY_SIZE,
                    (header.num_keys - pos) * BPLUS_ENTRY_SIZE
                );
            }
            
            Serializer::serialize_value_fixed(key, page + BPLUS_HEADER_SIZE + 4 + pos * BPLUS_ENTRY_SIZE);
            std::memcpy(page + BPLUS_HEADER_SIZE + 4 + pos * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, &child_page_id, 4);
            
            header.num_keys++;
            write_header(page, header);
            pager_->write_page(parent_id, page);
        } else {
            // Split internal node
            throw std::runtime_error("Internal node split not implemented to keep it simple, please use a larger page size or implement it.");
        }
    }

public:
    BPlusTree(Pager* pager, uint32_t root_page_id = 0) : pager_(pager), root_page_id_(root_page_id) {
        if (pager_->get_num_pages() == 0) {
            root_page_id_ = pager_->allocate_page();
            char page[PAGE_SIZE] = {0};
            NodeHeader header = {true, 0, 0};
            write_header(page, header);
            pager_->write_page(root_page_id_, page);
        }
    }

    uint32_t get_root_page_id() const { return root_page_id_; }

    bool search(const Value& key, uint32_t& out_offset) const {
        uint32_t current_page_id = root_page_id_;
        char page[PAGE_SIZE];

        while (true) {
            pager_->read_page(current_page_id, page);
            NodeHeader header;
            read_header(page, header);

            if (header.is_leaf) {
                for (uint16_t i = 0; i < header.num_keys; ++i) {
                    Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + i * BPLUS_ENTRY_SIZE);
                    if (k == key) {
                        std::memcpy(&out_offset, page + BPLUS_HEADER_SIZE + i * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                        return true;
                    }
                }
                return false;
            } else {
                uint32_t next_page_id = 0;
                bool found = false;
                for (uint16_t i = 0; i < header.num_keys; ++i) {
                    Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + 4 + i * BPLUS_ENTRY_SIZE);
                    if (key < k) {
                        if (i == 0) {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                        } else {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (i - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (header.num_keys == 0) {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                    } else {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (header.num_keys - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                    }
                }
                current_page_id = next_page_id;
            }
        }
    }

    void insert(const Value& key, uint32_t row_offset) {
        uint32_t current_page_id = root_page_id_;
        char page[PAGE_SIZE];
        Vector<uint32_t> path;
        
        while (true) {
            path.push_back(current_page_id);
            pager_->read_page(current_page_id, page);
            NodeHeader header;
            read_header(page, header);
            
            if (header.is_leaf) {
                if (header.num_keys < BPLUS_MAX_KEYS) {
                    uint16_t pos = 0;
                    while (pos < header.num_keys) {
                        Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + pos * BPLUS_ENTRY_SIZE);
                        if (key < k) break;
                        if (key == k) throw std::runtime_error("Duplicate primary key");
                        pos++;
                    }
                    if (pos < header.num_keys) {
                        std::memmove(
                            page + BPLUS_HEADER_SIZE + (pos + 1) * BPLUS_ENTRY_SIZE,
                            page + BPLUS_HEADER_SIZE + pos * BPLUS_ENTRY_SIZE,
                            (header.num_keys - pos) * BPLUS_ENTRY_SIZE
                        );
                    }
                    Serializer::serialize_value_fixed(key, page + BPLUS_HEADER_SIZE + pos * BPLUS_ENTRY_SIZE);
                    std::memcpy(page + BPLUS_HEADER_SIZE + pos * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, &row_offset, 4);
                    
                    header.num_keys++;
                    write_header(page, header);
                    pager_->write_page(current_page_id, page);
                } else {
                    // Split leaf
                    uint32_t new_page_id = pager_->allocate_page();
                    char new_page[PAGE_SIZE] = {0};
                    NodeHeader new_header = {true, 0, header.next_leaf};
                    header.next_leaf = new_page_id;
                    
                    uint16_t split_point = header.num_keys / 2;
                    uint16_t move_count = header.num_keys - split_point;
                    
                    std::memcpy(
                        new_page + BPLUS_HEADER_SIZE,
                        page + BPLUS_HEADER_SIZE + split_point * BPLUS_ENTRY_SIZE,
                        move_count * BPLUS_ENTRY_SIZE
                    );
                    new_header.num_keys = move_count;
                    header.num_keys = split_point;
                    
                    write_header(page, header);
                    write_header(new_page, new_header);
                    
                    pager_->write_page(current_page_id, page);
                    pager_->write_page(new_page_id, new_page);
                    
                    Value split_key = Serializer::deserialize_value_fixed(new_page + BPLUS_HEADER_SIZE);
                    path.pop_back(); // remove current_page_id
                    insert_into_internal(path, split_key, new_page_id);
                    
                    // Re-insert the original key since it might belong to the new page
                    insert(key, row_offset);
                }
                return;
            } else {
                uint32_t next_page_id = 0;
                bool found = false;
                for (uint16_t i = 0; i < header.num_keys; ++i) {
                    Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + 4 + i * BPLUS_ENTRY_SIZE);
                    if (key < k) {
                        if (i == 0) {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                        } else {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (i - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (header.num_keys == 0) {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                    } else {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (header.num_keys - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                    }
                }
                current_page_id = next_page_id;
            }
        }
    }
    
    // Very naive delete: we don't merge nodes.
    void remove(const Value& key) {
        uint32_t current_page_id = root_page_id_;
        char page[PAGE_SIZE];

        while (true) {
            pager_->read_page(current_page_id, page);
            NodeHeader header;
            read_header(page, header);

            if (header.is_leaf) {
                for (uint16_t i = 0; i < header.num_keys; ++i) {
                    Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + i * BPLUS_ENTRY_SIZE);
                    if (k == key) {
                        if (i < header.num_keys - 1) {
                            std::memmove(
                                page + BPLUS_HEADER_SIZE + i * BPLUS_ENTRY_SIZE,
                                page + BPLUS_HEADER_SIZE + (i + 1) * BPLUS_ENTRY_SIZE,
                                (header.num_keys - i - 1) * BPLUS_ENTRY_SIZE
                            );
                        }
                        header.num_keys--;
                        write_header(page, header);
                        pager_->write_page(current_page_id, page);
                        return;
                    }
                }
                return; // not found
            } else {
                uint32_t next_page_id = 0;
                bool found = false;
                for (uint16_t i = 0; i < header.num_keys; ++i) {
                    Value k = Serializer::deserialize_value_fixed(page + BPLUS_HEADER_SIZE + 4 + i * BPLUS_ENTRY_SIZE);
                    if (key < k) {
                        if (i == 0) {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                        } else {
                            std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (i - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (header.num_keys == 0) {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE, 4);
                    } else {
                        std::memcpy(&next_page_id, page + BPLUS_HEADER_SIZE + 4 + (header.num_keys - 1) * BPLUS_ENTRY_SIZE + BPLUS_KEY_SIZE, 4);
                    }
                }
                current_page_id = next_page_id;
            }
        }
    }
};

} // namespace dbms
