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
#include <libsdb/error.hpp>
#include <string_view>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include <string_view>

namespace {

    /* points to data in .debug_info */
    class cursor {
        public:
            /* start from a point of data to the end */
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
            /* and advance cursor by sizeof(T) amount */
            template<class T>
            T fixed_int() {
                auto t = sdb::from_bytes<T>(pos_);
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

            /* handles every possible DWARF form */
            /* skip past DIE attributes dependent on the size of their forms */
            void skip_form(std::uint64_t form) {
                switch (form) {
                    /* reduces repetition by grouping common cases together */
                    case DW_FORM_flag_present:
                        break;
                    case DW_FORM_data1:
                    case DW_FORM_ref1:
                    case DW_FORM_flag:
                        pos_ += 1; 
                        break;
                    case DW_FORM_data2:
                    case DW_FORM_ref2:
                        pos_ += 2; 
                        break;
                    case DW_FORM_data4:
                    case DW_FORM_ref4:
                    case DW_FORM_ref_addr:
                    case DW_FORM_sec_offset:
                    case DW_FORM_strp:
                        pos_ += 4; 
                        break;
                    case DW_FORM_data8:
                    case DW_FORM_addr:
                        pos_ += 8; 
                        break;
                    /* for the cases below parse but don't retrieve the data */
                    case DW_FORM_sdata:
                        sleb128(); 
                        break;
                    case DW_FORM_udata:
                    case DW_FORM_ref_udata:
                        uleb128();  
                        break;
                    case DW_FORM_block1:
                        pos_ += u8();
                        break;
                    case DW_FORM_block2:
                        pos_ += u16();
                        break;
                    case DW_FORM_block4:
                        pos_ += u32();
                        break;
                    case DW_FORM_block:
                    case DW_FORM_exprloc:
                        pos_ += uleb128();
                        break;
                    case DW_FORM_string:
                        /* iterate until you hit the null terminator */
                        while (!finished() && *pos_ != std::byte(0)) {
                            ++pos_;
                        }
        
                        /* go past the null terminator */
                        ++pos_;
                        break;
                    case DW_FORM_indirect:
                        skip_form(uleb128());
                        break;
                    default: sdb::error::send("Unrecognized DWARF form");
            }
        };

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
    class die;
    class compile_unit;
    class die;


    /* ULEB128 encoding for both fields */
    struct attr_spec {
        std::uint64_t attr; /* value of the attribute */
        std::uint64_t form; /* specifies attribute's encoding */
    };

    struct abbrev {
        std::uint64_t code; //ULEB 128 reference table code
        std::uint64_t tag; //ULEB 128 entry tag code
        bool has_children;
        std::vector<attr_spec> attr_specs; 

    };

    class attr {
        public:
            attr(const compile_unit* cu, std::uint64_t type, std::uint64_t form, const std::byte* attr_loc) :
            cu_(cu), type_(type), form_(form), attr_loc_(attr_loc) {}

            /* get functions */
            std::uint64_t name() const { return type_; }
            std::uint64_t form() const { return form_; }

            /* handles retrieving value based on the type of the attribute */
            
            /* retrieves value of address DIEs */
            file_addr as_address() const;

            /* retrieves value of section offset DIEs */
            std::uint32_t as_section_offset() const;

            /* retrieves value of block DIEs */
            span<const std::byte> as_block() const;
            std::uint64_t as_int() const;
            std::string_view as_string() const;
            
            /* parse DIE at that offset */
            die as_reference() const;
             
        private:
            std::uint64_t type_;
            std::uint64_t form_;

            //for internal use
            const compile_unit* cu_;
            const std::byte* attr_loc_;
    };


    class compile_unit {
        public:
            /* returns the root die - 11 bytes accounted for from header size */
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
            * @param attr_locs  location of DIE attributes in the compile unit
            */
            die(const std::byte* pos, const compile_unit* cu, const abbrev* abbrev,
                const std::byte* next, std::vector<const std::byte*> attr_locs) :
                    pos_(pos),
                    cu_(cu),
                    abbrev_(abbrev),
                    next_(next),
                    attr_locs_(std::move(attr_locs)) {}
            

            /* get functions */
            const compile_unit* cu() const { return cu_; }
            const abbrev* abbrev_entry() const { return abbrev_; }
            const std::byte* position() const { return pos_; }
            const std::byte* next() const { return next_; }

            /* wrap a DIE and provide iterator to the DIE's children */
            class children_range;
            children_range children() const;

            /* checks if DIE has an attribute of that type */
            /* check attribute specifications and find corresponding attribute */
            bool contains(std::uint64_t attr) const;

            /* retrieves attribute value */
            attr operator[](std::uint64_t attr) const;

        private:
            const std::byte* pos_ = nullptr;
            const compile_unit* cu_ = nullptr; //associated compile unit
            const abbrev* abbrev_ = nullptr; //associated abbreviation block 
            const std::byte* next_ = nullptr; //points to next die
            std::vector<const std::byte*> attr_locs_;
    };

    /* stores range of children dies and die tree iteration */
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


                    /* access wrapped DIE */
                    const die& operator*() const { return *die_;}
                    const die* operator->() const { return &die_.value();}


                    /* when we have finished iterating */
                    bool operator==(const iterator &rhs) const;
                    bool operator!=(const iterator &rhs) const {
                        return !(*this==rhs);
                    }

                    /* advancing to next DIE */
                    iterator& operator++(); //pre-increment
                    iterator operator++(int); //post-increment


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