#include "common/Vector.h"
#include <iostream>
#include <cassert>
#include <string>

void test_vector() {
    dbms::Vector<int> v;
    assert(v.empty());
    assert(v.size() == 0);

    v.push_back(10);
    v.push_back(20);
    v.push_back(30);

    assert(v.size() == 3);
    assert(v[0] == 10);
    assert(v[1] == 20);
    assert(v[2] == 30);

    v.pop_back();
    assert(v.size() == 2);
    assert(v[1] == 20);

    v.erase(0);
    assert(v.size() == 1);
    assert(v[0] == 20);

    dbms::Vector<std::string> vs;
    vs.push_back("hello");
    vs.emplace_back("world");
    assert(vs.size() == 2);
    assert(vs[0] == "hello");
    assert(vs[1] == "world");

    std::cout << "Vector tests passed!" << std::endl;
}

int main() {
    test_vector();
    return 0;
}
