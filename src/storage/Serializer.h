#pragma once
#include "core/Types.h"
#include <cstring>
#include <string>
#include <cstdint>

namespace dbms {

// Fixed size of a serialized key in B+ Tree (1 byte type + 2 bytes len + 256 bytes data = 259)
constexpr size_t BPLUS_KEY_SIZE = 259;

class Serializer {
public:
    static void serialize_value_fixed(const Value& val, char* dest) {
        std::memset(dest, 0, BPLUS_KEY_SIZE);
        char type = val.type == DataType::INT ? 0 : 1;
        dest[0] = type;
        
        if (val.type == DataType::INT) {
            std::memcpy(dest + 1, &val.int_val, sizeof(int));
        } else {
            uint16_t len = val.str_val.length();
            if (len > 256) len = 256;
            std::memcpy(dest + 1, &len, sizeof(uint16_t));
            std::memcpy(dest + 3, val.str_val.c_str(), len);
        }
    }

    static Value deserialize_value_fixed(const char* src) {
        char type = src[0];
        if (type == 0) {
            int val;
            std::memcpy(&val, src + 1, sizeof(int));
            return Value(val);
        } else {
            uint16_t len;
            std::memcpy(&len, src + 1, sizeof(uint16_t));
            if (len > 256) len = 256; // safeguard
            std::string str(src + 3, len);
            return Value(str);
        }
    }
};

} // namespace dbms
