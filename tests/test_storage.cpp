#include "storage/Pager.h"
#include "storage/TableFile.h"
#include <cassert>
#include <iostream>
#include <cstdio>
#include <cstdint>
#include <cstring>

void test_storage() {
    using namespace dbms;

    // Test Pager
    std::string idx_file = "test.idx";
    {
        Pager pager(idx_file);
        assert(pager.get_num_pages() == 0);
        
        uint32_t p1 = pager.allocate_page();
        assert(p1 == 0);
        assert(pager.get_num_pages() == 1);
        
        char buf[PAGE_SIZE] = {0};
        strcpy(buf, "hello pager");
        pager.write_page(p1, buf);
        
        char read_buf[PAGE_SIZE];
        pager.read_page(p1, read_buf);
        assert(strcmp(read_buf, "hello pager") == 0);
    }
    std::remove(idx_file.c_str());

    // Test TableFile
    std::string dat_file = "test.dat";
    {
        TableFile tf(dat_file);
        
        Row r1;
        r1.values.push_back(Value(1001));
        r1.values.push_back(Value(std::string("peter")));
        
        uint32_t off1 = tf.insert_row(r1);
        
        Row r2;
        r2.values.push_back(Value(1002));
        r2.values.push_back(Value(std::string("mary")));
        uint32_t off2 = tf.insert_row(r2);
        
        Row fetched;
        bool ok = tf.get_row(off1, fetched);
        assert(ok);
        assert(fetched.values[0].int_val == 1001);
        assert(fetched.values[1].str_val == "peter");
        
        tf.delete_row(off1);
        
        ok = tf.get_row(off1, fetched);
        assert(!ok); // Should be deleted
        
        Vector<Row> all_rows;
        Vector<uint32_t> all_offs;
        tf.scan_all_rows(all_rows, all_offs);
        
        assert(all_rows.size() == 1); // Only r2 should remain
        assert(all_rows[0].values[0].int_val == 1002);
    }
    std::remove(dat_file.c_str());
    
    std::cout << "Storage tests passed!" << std::endl;
}

int main() {
    test_storage();
    return 0;
}
