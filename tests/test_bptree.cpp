#include "storage/BPlusTree.h"
#include <cassert>
#include <iostream>
#include <cstdio>

void test_bptree() {
    using namespace dbms;

    std::string idx_file = "test_btree.idx";
    std::remove(idx_file.c_str());
    
    {
        Pager pager(idx_file);
        BPlusTree tree(&pager);
        
        // Insert a few keys
        tree.insert(Value(10), 100);
        tree.insert(Value(20), 200);
        tree.insert(Value(5), 50);
        
        uint32_t offset;
        assert(tree.search(Value(10), offset) && offset == 100);
        assert(tree.search(Value(20), offset) && offset == 200);
        assert(tree.search(Value(5), offset) && offset == 50);
        assert(!tree.search(Value(15), offset));
        
        // Cause a split (MAX_KEYS is around 15, so insert 20 keys)
        for (int i = 100; i < 120; ++i) {
            tree.insert(Value(i), i * 10);
        }
        
        for (int i = 100; i < 120; ++i) {
            assert(tree.search(Value(i), offset) && offset == static_cast<uint32_t>(i * 10));
        }
        
        // Remove a key
        tree.remove(Value(10));
        assert(!tree.search(Value(10), offset));
    }
    
    std::remove(idx_file.c_str());
    std::cout << "BPlusTree tests passed!" << std::endl;
}

int main() {
    test_bptree();
    return 0;
}
