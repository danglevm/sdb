#ifndef SDB_TYPES_HPP
#define SDB_TYPES_HPP

#include <cstring>
#include <array>
#include <libsdb/types.hpp>

namespace sdb {
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
}

#endif