#ifndef SDB_DWARF_HPP
#define SDB_DWARF_HPP

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <libsdb/elf.hpp>
#include <libsdb/detail/dwarf.h>
#include <libsdb/types.hpp>
#include <libsdb/bits.hpp>
#include <string_view>
#include <algorithm>
#include <unordered_map>


namespace {
    class cursor {
        public:
            explicit cursor(sdb::span<std::byte> data)
             : data_(data), pos_(data.begin()){}

            cursor& operator++() {++pos_; return *this;}
            cursor& operator+=(std::size_t size) { pos_ += size; return *this;}

            const std::byte* get_pos() const { return pos_; }

            /* cursor has read all of data */
            bool finished() const {
                return pos_ >= data_.end();
            }

            /* parse fix-width integers at position of cursor */
            template<class T>
            T fixed_int() {
                auto t = sdb::from_bytes<T>(pos_);
                //advancing cursor
                pos_ += sizeof(T);
                return t;
            }
            
            /* Parse certain types of integer at position of cursor */
            std::uint8_t u8() { return fixed_int<uint8_t>(); }
            std::uint16_t u16() { return fixed_int<uint16_t>(); }
            std::uint32_t u32() { return fixed_int<uint32_t>(); }
            std::uint64_t u64() { return fixed_int<uint64_t>(); }

            std::int8_t s8() { return fixed_int<int8_t>(); }
            std::int16_t s16() { return fixed_int<int16_t>(); }
            std::int32_t s32() { return fixed_int<int32_t>(); }
            std::int64_t s64() { return fixed_int<int64_t>(); }

            /* Parsing strings */
            std::string_view string() {
                //0 byte character is null-character. find the end character of the string to calculate length
                auto null_terminator = std::find(pos_, data_.end(), std::byte{0});
                std::string_view ret{reinterpret_cast<const char*>(pos_), null_terminator - pos_};

                //end dwarf file reading
                pos_ = null_terminator + 1;
                return ret;
            }

            /* Parse unsigned leb128 */
            std::uint64_t uleb128() {
                std::uint64_t res = 0;
                int shift = 0; //shift amount
                std::uint8_t byte = 0;

                /* since the start bit of a group of 7 bits is non-zero */
                do {
                    byte = u8();
                    // mask off the first bit since we only deal in groups of 7 bits
                    auto masked = static_cast<uint64_t>(byte & 0x7f);
                    res |= (masked << shift);
                    shift += 7;
                } while ((byte & 0x80) != 0);

                return res;
            }

            /* Parse signed leb128 */
            std::uint64_t sleb128() {
                std::uint64_t res = 0;
                int shift = 0; //shift amount
                std::uint8_t byte = 0;

                /* since the start bit of a group of 7 bits is non-zero */
                do {
                    byte = u8();
                    // mask off the first bit since we only deal in groups of 7 bits
                    auto masked = static_cast<uint64_t>(byte & 0x7f);
                    res |= (masked << shift);
                    shift += 7;
                } while ((byte & 0x80) != 0);

                return res;
            }

        private:
            /* represents data range being looked at */
            sdb::span<const std::byte> data_;

            /* cursor's current position */
            const std::byte * pos_;
    };
}

namespace sdb {
    class elf;
    class dwarf{
        public:
            dwarf(const elf& parent);
            const elf* elf() const { return elf_;};
            const std::unordered_map<std::uint64_t, sdb::abbrev> & get_abbrev_table(std::size_t offset); 

        private:
            const elf* elf_;
            /*
            * @param size_t        byte offset from start of .debug_abbrev section
            * @param unordered_map maps abbreviation code integer to abbreviation entries
            */
            std::unordered_map<std::size_t, 
                std::unordered_map<std::uint64_t, sdb::abbrev>> abbrev_tables_;

    };
}

#endif