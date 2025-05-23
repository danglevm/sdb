#ifndef SDB_DWARF_HPP
#define SDB_DWARF_HPP

#include "types.hpp"
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <libsdb/elf.hpp>
#include <libsdb/detail/dwarf.h>
#include <libsdb/types.hpp>
#include <libsdb/bits.hpp>
#include <string_view>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>


namespace {

    /* points to data in .debug_info */
    class cursor {
        public:
            explicit cursor(sdb::span<const std::byte> data)
             : data_(data), pos_(data.begin()){}

            cursor& operator++() {++pos_; return *this;}
            cursor& operator+=(std::size_t size) { pos_ += size; return *this;}

            const std::byte* get_pos() const { return pos_; }

            /* cursor has read all of data */
            bool finished() const {
                return pos_ >= data_.end();
            }

            /* parse fix-width integers at position of DWARF cursor */
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

                /* check if all bits of res (64 bits) are filled and perform 
                sign extension if 6th bit is 1 - negative number */
                if ((shift < sizeof(res) * 8) && (byte & 0x40)) {
                    //fill the remaining starting bits with 1
                    res |= (~static_cast<std::uint64_t>(0) << shift);
                }
                return res;
            }

            /* skip over attributes */
            /* handles every possible DWARF form */
            void skip_form(std::uint64_t form);

        private:
            /* represents data range being looked at */
            sdb::span<const std::byte> data_;

            /* current position of DWARF cursor */
            const std::byte * pos_;
    };
}

namespace sdb {
    class elf;
    class dwarf;
    /* ULEB128 encoding for both fields */
    struct attr_spec {
        std::uint64_t attr;
        std::uint64_t form; /* specifies attribute's encoding */
    };

    struct abbrev {
        std::uint64_t code; //ULEB 128 reference table code
        std::uint64_t tag; //ULEB 128 entry tag code
        bool has_children;
        std::vector<attr_spec> attr_specs; 

    };

    class die;
    class compile_unit {
        public:
            die root() const;

            compile_unit(sdb::dwarf& parent, sdb::span<const std::byte> data, std::size_t abbrev_offset) :
            parent_(&parent), data_(data), abbrev_offset_(abbrev_offset) {}
            sdb::span<const std::byte> data() const { return data_;} 
            const sdb::dwarf* parent() const { return parent_;}

            /* retrieves the abbrev table for this compile unit */
            const std::unordered_map<std::uint64_t, sdb::abbrev>&
            abbrev_table() const;

            /* Handling DIEs */


        private:
            sdb::dwarf* parent_;
            sdb::span<const std::byte> data_; //data stored inside .debug_info about compiled unit
            std::size_t abbrev_offset_;

    };

    class die {
        public:
            /* for null DIEs */
            /* next points to the next DIE in the section */
            explicit die(const std::byte * next) : next_(next) {}
            /* for non-null DIEs*/
            /*
            * @param pos        position of the DIE in .debug_info
            * @param cu         compile unit associated with DIE
            * @param abbrev     abbreviation entry associated with DIE
            * @param next       next DIE entry 
            * @param attr_locs  locations of DIE attributes
            */
            die(const std::byte* pos, const compile_unit* cu, const abbrev* abbrev,
                const std::byte* next, std::vector<const std::byte*> attr_locs) :
                    pos_(pos),
                    cu_(cu),
                    abbrev_(abbrev),
                    next_(next),
                    attr_locs_(std::move(attr_locs)) {}
            

            /* set-get functions */
            const compile_unit* cu() const { return cu_; }
            const abbrev* abbrev_entry() const { return abbrev_; }
            const std::byte* position() const { return pos_; }
            const std::byte* next() const { return next_; }

            /* wrap a DIE and provide iterators to the DIE's children */
            class children_range;
            children_range children() const;
            

        private:
            const std::byte* pos_ = nullptr;
            const compile_unit* cu_ = nullptr;
            const abbrev* abbrev_ = nullptr;
            const std::byte* next_ = nullptr;
            std::vector<const std::byte*> attr_locs_;


    };

    class die::children_range {
        public:
            children_range(die die) : die_(std::move(die)){}

            class iterator {
                public:
                    using value_type = die; /* type being iterated over */
                    using reference = const die&; /* type returned by operator* */
                    using pointer = const die*; /* type returned by operator-> */
                    using difference_type = std::ptrdiff_t; /* type returned by subtracting two iterators */
                    
                    /* only for moving forward, but support multi-passes */
                    using iterator_category = std::forward_iterator_tag;

                    iterator() = default;
                    iterator(const iterator&) = default;
                    iterator& operator=(const iterator&) = default;

                    explicit iterator(const die& die);

                    const die& operator*() const { return *die_;}
                    const die& operator->() const { return &die_.value();}

                    bool operator==(const iterator &rhs) const;
                    bool operator!=(const iterator &rhs) const {
                        return !(*this==rhs);
                    }

                    iterator& operator++();
                    iterator operator++(int);


                private:
                    std::optional<die> die_;


            };

            iterator begin() const {
                if(die_.abbrev_->has_children) {
                    return iterator{die_};
                }
                return end();
            }

            /* returns empty iterator marking end of storage */
            iterator end() const {
                return iterator {};
            }
        private:
            die die_;
    };
    class dwarf {
        public:
            dwarf(const elf& parent);
            const elf* get_elf() const { return elf_;};

            /*
            * @param offset offset into .debug_abbrev section 
            * @return abbreviation table at the offset
            */
            const std::unordered_map<std::uint64_t, sdb::abbrev>& 
            get_abbrev_table(std::size_t offset); 

            const std::vector<std::unique_ptr<sdb::compile_unit>>& 
            compile_units() const { return compile_units_; }

        private:
            const elf* elf_;
            /*
            * @param size_t        byte offset from start of .debug_abbrev section
            * @param unordered_map maps abbreviation code integer to abbreviation entries
            */
            std::unordered_map<std::size_t, 
                std::unordered_map<std::uint64_t, sdb::abbrev>> abbrev_tables_;

            std::vector<std::unique_ptr<sdb::compile_unit>> compile_units_;

    };




}

#endif