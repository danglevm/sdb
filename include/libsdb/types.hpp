#ifndef SDB_TYPES_HPP
#define SDB_TYPES_HPP

#include <cstring>
#include <cstdint>
#include <array>
#include <vector>
#include <cassert>

namespace sdb {

    class file_addr;
    class elf;


    enum class stoppoint_mode{ 
        write,
        read_write,
        execution
    };

    using byte64 = std::array<std::byte, 8>;
    using byte128 = std::array<std::byte, 16>;

    /* initialized to 0. Up to the size of From to allow cast types smaller than 128/64 bytes */

    /* cast to 64 bits - 16 bytes of value. For MM registers */
    template<typename From>
    byte64 as_byte64(From from) {
        byte64 To{};
        std::memcpy(&To, &from, sizeof(From));
        return To;
    }

    /* cast to 128 bits - 16 bytes of value. For XMM registers */
    template<typename From>
    byte128 as_byte128(From from) {
        byte128 To{};
        std::memcpy(&To, &from, sizeof(From));
        return To;
    }

    /* storing virtual addresses of breakpoints and programs */
    /* essentially a wrapper class for a std::uint64_t */
    /* processes */
    class virt_addr {
        public:
            virt_addr() = default;
            /* disallow implicit conversions */
            explicit virt_addr(std::uint64_t addr) : addr_(addr) {}

            std::uint64_t addr() const { return addr_; }

            file_addr convert_to_file_addr(const elf& obj) const;

            /* operators to overload - change by a certain offset */
            /* doing virt_addr = base + 0x200 would shift it by that much. Takes the value addr_ of base */
            //create a new object for assignment
            virt_addr operator+(std::int64_t offset) const {
                return virt_addr(addr_ + offset);
            }

            virt_addr operator-(std::int64_t offset) const {
                return virt_addr(addr_ - offset);
            }

            /* not returning a new object but modify so no const */
            virt_addr& operator+=(std::int64_t offset) {
                addr_ += offset;
                return *this; //allows chaining assignment
            }

            virt_addr& operator-=(std::int64_t offset) {
                addr_ -= offset;
                return *this; //allows chaining assignment
            }

            /* comparison operator */
            bool operator==(const virt_addr& other) const {
                return addr_ == other.addr_;
            }

            bool operator!=(const virt_addr& other) const {
                return addr_ != other.addr_;
            }

            bool operator>(const virt_addr& other) const {
                return addr_ > other.addr_;
            }

            bool operator>=(const virt_addr& other) const {
                return addr_ >= other.addr_;
            }

            bool operator<(const virt_addr& other) const {
                return addr_ < other.addr_;
            }

            bool operator<=(const virt_addr& other) const {
                return addr_ <= other.addr_;
            }

            file_addr converstion_to_file_addr(const elf& obj) const;

        private:
            std::uint64_t addr_= 0;
    };

    /* Stores ELF file addresses and routines for translating between virtual and file addresses */
    class file_addr {
        public:
            file_addr() = default;
            explicit file_addr(std::uint64_t addr, const elf& elf) : addr_(addr), elf_(&elf) {}

            const elf* get_elf_file() const { return elf_; } 
            std::uint64_t addr() const { return addr_; }

            /* convert stored address to virtual address with load bias */
            /* compute real virtual address */
            virt_addr convert_to_virt_addr() const;

            /* file address operator overloads */
            file_addr operator+(std::int64_t offset) const {
                return file_addr(addr_ + offset, *elf_);
            }

            file_addr operator-(std::int64_t offset) const {
                return file_addr(addr_ - offset, *elf_);
            }

            /* not returning a new object but modify so no const */
            file_addr& operator+=(std::int64_t offset) {
                addr_ += offset;
                return *this; //allows chaining assignment
            }

            file_addr& operator-=(std::int64_t offset) {
                addr_ -= offset;
                return *this; //allows chaining assignment
            }

            /* comparison operator */
            bool operator==(const file_addr& other) const {
                return addr_ == other.addr_ && elf_ == other.elf_;
            }

            bool operator!=(const file_addr& other) const {
                return addr_ != other.addr_ && elf_ == other.elf_;
            }

            bool operator>(const file_addr& other) const {
                assert(elf_ == other.elf_);
                return addr_ > other.addr_;
            }

            bool operator>=(const file_addr& other) const {
                assert(elf_ == other.elf_);
                return addr_ >= other.addr_;
            }

            bool operator<(const file_addr& other) const {
                assert(elf_ == other.elf_);
                return addr_ < other.addr_;
            }

            bool operator<=(const file_addr& other) const {
                assert(elf_ == other.elf_);
                return addr_ <= other.addr_;
            }
        private:
            std::uint64_t addr_ = 0;
            /* pointer to ELF file */
            const elf* elf_ = nullptr;
    };

    /* Stores ELF file offset and routines for translating between virtual and file offset */
    class file_offset {
        public:
            file_offset() = default;

            file_offset(std::uint64_t off, const elf& elf) : off_(off), elf_(&elf) {}

            const elf* get_elf_file() const { return elf_; } 
            std::uint64_t off() const { return off_; }

        private:
            std::uint64_t off_ = 0;
            /* pointer to ELF file */
            const elf* elf_ = nullptr;
    };  

    /* represents an existing region of memory */
    /* wrapper around general memory allocation regions */
    /* this is similar to the C++20 implementation, but we are doing C++ 17 */
    template<typename T>
    class span {
        public:
            //default constructor for empty span
            span() = default;

            //pointer and size
            span(T* data, std::size_t size) : data_(data), size_(size) {}
            
            // start and end pointer span
            span(T* data, T* end) : data_(data), size_(end - data){}
            template <typename U>
            span(const std::vector<U>& vec) : data_(vec.data(), size_(vec.size())) {}

            //get start of span
            T* begin() const {return data_;}

            //get end of span
            T* end() const {return data_ + size_;} 

            //get size of span
            std::size_t size() const {return size_;}
            T& operator[] (std::size_t n) { return *(data_ + n);}

        private:
            T* data_ = nullptr;
            std::size_t size_ = 0;
    };
}

#endif