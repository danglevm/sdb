#ifndef SDB_REGISTER_INFO_HPP
#define SDB_REGISTER_INFO_HPP

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <sys/user.h>

namespace sdb {
    
    /* unique register enum value */
    enum class register_id {
        #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) name
        #include <libsdb/detail/registers.inc>
        #undef DEFINE_REGISTER
    };


    /* types of x86_64 registers */
    enum class register_type {
        gpr, 
        sub_gpr,
        fpr,
        dr
    };

    /* different register interpretations */
    enum class register_format {
        uint, /* integer arith, memory addressing, control float*/
        double_float, /* double-precision floating point */
        long_double,
        vector /* SIMD operations */
    };

    /* based on sys/user.h */
    struct register_info {
        register_id id;
        std::string_view name;
        std::int32_t dwarf_id;
        std::size_t size; /* size in bytes */
        std::size_t offset; /* byte offset into user struct */
        register_type type;
        register_format format;
    };

    /* info of every register in the system */
    inline constexpr const register_info g_register_infos[] {

    };
}

#endif