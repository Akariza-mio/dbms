#include "storage/serializer.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <bit>
#include <string>
#include <utility>

namespace minidb::serialization{
    namespace{
        //little-endian
        void append_u8(byte_vector& output,std::uint8_t value){
            output.push_back(static_cast<std::byte>(value));
        }
        void append_u32(byte_vector& output,std::uint32_t value){
            std::byte tp;
            for(std::uint32_t i = 0; i < 32; i += 8){
                tp = static_cast<std::byte>((value >> i) & 0xFF);
                output.push_back(tp);
            }
        }
        void append_u64(byte_vector& output,std::uint64_t value){
            std::byte tp;
            for(std::uint32_t i = 0; i < 64; i += 8){
                tp = static_cast<std::byte>((value >> i) & 0xFF);
                output.push_back(tp);
            }
        }
        //read bytes only
        std::uint8_t read_u8(const byte_vector& input,std::size_t& offset){
            if(offset > input.size() || 1 > input.size() - offset){
                throw std::out_of_range("read_u8: offset out of range");
            }
            std::uint8_t res = std::to_integer<std::uint8_t>(input[offset]);
            offset += 1;
            return res;
        }
        std::uint32_t read_u32(const byte_vector& input,std::size_t& offset){
            if(offset > input.size() || 4 > input.size() - offset){
                throw std::out_of_range("read_u32: offset out of range");
            }
            std::uint32_t res = 0;
            for(std::uint32_t i = 0; i < 32; i += 8){
                res |= (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(input[offset + i / 8])) << i);
            }
            offset += 4;
            return res;
        }
        std::uint64_t read_u64(const byte_vector& input,std::size_t& offset){
            if(offset > input.size() || 8 > input.size() - offset){
                throw std::out_of_range("read_u64: offset out of range");
            }
            std::uint64_t res = 0;
            for(std::uint32_t i = 0; i < 64; i += 8){
                res |= (static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(input[offset + i / 8])) << i);
            }
            offset += 8;
            return res;
        }
        enum class ValueTag : std::uint8_t{
            INT = 1,
            FLOAT = 2,
            STRING = 3
        };
        //read a value from the byte file
        Value read_value(const byte_vector& input,std::size_t& offset){
            const std::uint8_t tag = read_u8(input,offset);
            const ValueTag value_tag = static_cast<ValueTag>(tag);
            switch(value_tag){
                case ValueTag::INT:{
                    const std::uint32_t bits = read_u32(input,offset);
                    const std::int32_t value = std::bit_cast<std::int32_t>(bits);
                    return Value(value);
                }
                case ValueTag::FLOAT:{
                    const std::uint64_t bits = read_u64(input,offset);
                    const double value = std::bit_cast<double>(bits);
                    return Value(value);
                }
                case ValueTag::STRING:{
                    const std::uint32_t len = read_u32(input,offset);
                    if(len > Value::MAX_STRING_LENGTH){
                        throw std::invalid_argument("String length exceeds maximum length of 256 bytes");
                    }
                    const std::size_t len_sizet = static_cast<std::size_t>(len);
                    if(offset > input.size() || len_sizet > input.size() - offset){
                        throw std::out_of_range("read_value: offset out of range for string");
                    }
                    std::string str;
                    str.reserve(len_sizet);
                    for(std::uint32_t i = 0; i < len_sizet; ++i){
                        str.push_back(static_cast<char>(read_u8(input,offset)));
                    }
                    return Value(str);
                }
                default:
                    throw std::invalid_argument("Unknown ValueTag");
            }
        }
    }
    byte_vector serialize_value(const Value& value){
        byte_vector output;
        switch(value.get_type()){
            case DataType::INT:{
                append_u8(output,static_cast<std::uint8_t>(ValueTag::INT));
                const std::uint32_t bits = std::bit_cast<std::uint32_t>(value.as_int());
                append_u32(output,bits);
                break;
            }
            case DataType::FLOAT:{
                //keep the FLOAT value as a 64-bit representation
                append_u8(output,static_cast<std::uint8_t>(ValueTag::FLOAT));
                const std::uint64_t bits = std::bit_cast<std::uint64_t>(value.as_float());
                append_u64(output,bits);
                break;
            }
            case DataType::STRING:{
                append_u8(output,static_cast<std::uint8_t>(ValueTag::STRING));
                const std::string& str = value.as_string();
                append_u32(output,static_cast<std::uint32_t>(str.length()));
                for(const unsigned char c : str){
                    append_u8(output,c);
                }
                break;
            }   
        }
        return output;
    }
    Value deserialize_value(const byte_vector& bytes){
        std::size_t offset = 0;
        Value value = read_value(bytes, offset);
        //check if there are extra bytes after the serialized Value
        if (offset != bytes.size()) {
            throw std::runtime_error(
                "Extra bytes after serialized Value"
            );
        }
        return value;
    }
}