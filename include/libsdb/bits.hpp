#ifndef SDB_BITS_HPP
#define SDB_BITS_HPP

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <string_view>

namespace sdb {

    /* converts raw byte sequence into strongly-typed object */
    template <typename To>
    To from_bytes(const std::byte* bytes) {
        To ret;
        std::memcpy(&ret, bytes, sizeof(To)); //safe memory copy alignment
        return ret;
    }

    inline std::string_view to_string_view (const std::byte * data, std::size_t size) {
        return { reinterpret_cast<const char *>(data), size};
    }

    /*  std::vector<std::byte> */
    inline std::string_view to_string_view (const std::vector<std::byte>& data) {
        return to_string_view(data.data(), data.size());
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