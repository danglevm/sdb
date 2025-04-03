#ifndef SDB_PARSE_HPP
#define SDB_PARSE_HPP

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <array> 
#include <cstddef>

namespace sdb {
    /* returns either I or nothing */
    /* converts string to some integer type or nothing */
    template<typename I>
    std::optional<I> to_integral(std::string_view sv, int base = 10) {
        auto begin = sv.begin();

        /* for parsing hex input */
        /* hex base is 16 so to allow a 0x prefix, we have to skip past it */
        if (base == 16 and sv.size() > 1 and begin[0] == '0' and begin[1] == 'x') {
            begin += 2;
        }
        
        I ret;
        auto result = std::from_chars(begin, sv.end(), ret, base);

        //from_chars succeed even if the entire string hasn't been read. 
        // Return an empty optional if some input remains
        if (result.ptr != sv.end()) {
            return std::nullopt;
        }

        return ret;
    }

    /* called only if std::byte is specified as template argument */
    /* same function as above, converts string to integer */
    template<>
    inline std::optional<std::byte> to_integral(std::string_view sv, int base) {
        auto uint8= to_integral<std::uint8_t>(sv, base);
        if (uint8) return static_cast<std::byte>(*uint8);
        return std::nullopt;
    }
    
    /* converts to floating point num */
    template<typename F>
    std::optional<F> to_float(std::string_view sv) {
        F ret;
        auto result = std::from_chars(sv.begin(), sv.end(), ret);

        if (result.ptr != sv.end()) {
            return std::nullopt;
        }

        return ret;
    }

    /* takes in a comma-separated list of hex integer with leading 0x, with square brackets */
    template<std::size_t N>
    auto parse_vector(std::string_view text) { 
        /* call it everytime we make it incorrect */
        auto invalid = [] { sdb::error::send("Invalid format"); };

        std::array<std::byte, N> bytes;
        const char * c = text.data();

        //first char
        if (*c++ != '[') invalid ();

        //loop over the first N-1 elements, parsing an element with a comma each time.
        for (auto i = 0; i < N - 1; ++i) {
            bytes[i] = to_integral<std::byte>({c, 4}, 16).value();
            c += 4;
            if (*c++ != ',') invalid();
        }

        /* deal with the last byte */
        bytes[N - 1] = to_integral<std::byte>({c, 4}, 16).value();
        c += 4;

        //last char
        if (*c++ != ']') invalid();
        if (c != text.end()) invalid();

        return bytes;
    }

    /**
    * Returns a vector with a collectio of std::byte
    * takes in a comma-separated list of hex integer with leading 0x, with square brackets
    */
    //overload that reads value until it hits a ']'
    inline auto parse_vector(
        std::string_view text) {
            auto invalid = [] { sdb::error::send("Invalid format"); };

            std::vector<std::byte> bytes;

            //first element of the string
            const char* c = text.data();

            //starting character
            if (*c++ != '[') invalid();

            while (*c != ']') {
                auto byte = sdb::to_integral<std::byte>({c, 4}, 16);
                
                //calling on std::optional
                bytes.push_back(byte.value());
                c += 4;

                if (*c == ',') {
                    ++c;
                } else if (*c != ']') {
                    invalid();
                }
            }

            //check if we have consumed all the data
            if (++c != text.end()) {
                invalid();
            }

            return bytes;
        }
}

#endif