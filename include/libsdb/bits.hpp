#ifndef SDB_BITS_HPP
#define SDB_BITS_HPP

#include <cstddef>
#include <cstring>

namespace sdb {

    /* converts raw byte sequence into strongly-typed object */
    template <typename To>
    To from_bytes(const std::byte* bytes) {
        To ret;
        std::memcpy(&ret, bytes, sizeof(To)); //safe memory copy alignment
        return ret;
    }

    /* for read and write-only operations */
    template<typename From>
    std::byte* as_bytes(From& from) {
        return reinterpret_cast<std::byte*>(&from);
    }
    
    /* overloaded const function for read-only operation */
    template<typename From>
    const std::byte* as_bytes(const From& from) {
        return reinterpret_cast<const std::byte*>(&from);
    }
}

#endif