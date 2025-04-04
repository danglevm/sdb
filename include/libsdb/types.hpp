#ifndef SDB_TYPES_HPP
#define SDB_TYPES_HPP

#include <cstring>
#include <cstdint>
#include <array>
#include <vector>
#include <libsdb/types.hpp>

namespace sdb {

    enum class stoppoint_mode{ 
        read,
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

            std::uint64_t addr() const {
                return addr_;
            }

            /* operators to overload - change by a certain offset */
            /* doing virt_addr = base + 0x200 would shift it by that much. Takes the value addr_ of base */
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

        private:
            std::uint64_t addr_= 0;
    };

    /* represents an existing region of memory */
    /* wrapper around general memory allocation regions */
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