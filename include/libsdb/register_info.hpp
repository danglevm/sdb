#ifndef SDB_REGISTER_INFO_HPP
#define SDB_REGISTER_INFO_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <string_view>
#include <sys/user.h>
#include <algorithm>
#include <libsdb/error.hpp>

/* 
*
*   STORES REGISTER INFO   
*
*/
namespace sdb {
    
    /* unique register enum value */
    enum class register_id {
        #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) name
        #include <libsdb/detail/registers.inc>
        //#include "registers.inc"
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
        register_id id; /* name in user struct */
        std::string_view name;
        std::int32_t dwarf_id; 
        std::size_t size; /* size of register in bytes */
        std::size_t offset; /* byte offset into user struct */
        register_type type;
        register_format format;
    };

    /* info of every register in the system */
    inline constexpr const register_info g_register_infos[] = {
        #define DEFINE_REGISTER(name,dwarf_id,size,offset,type,format) \
            { register_id::name, #name, dwarf_id, size, offset, type, format }
        #include <libsdb/detail/registers.inc>
        // #include "registers.inc"
        #undef DEFINE_REGISTER
    };

    /* get register metadata */
    template <typename F>
    const register_info& get_register_info_by(F f) {
        auto it = std::find_if(std::begin(g_register_infos), 
                            std::end(g_register_infos), f);
        
        if (it == std::end(g_register_infos)) {
            error::send("Can't find register info");
        }

        return *it;
    }

    /* get register metadata by fields */
    /* inline enables multiple function definitions to exist */

    /* @param register ID */
    inline const register_info& get_register_info_by_id(register_id id) {
        return get_register_info_by([id](auto &i) {return i.id == id;});
    }

    /* @param */
    inline const register_info& get_register_info_by_name(std::string_view& name) {
        return get_register_info_by([name](auto &i) {return i.name == name;});
    }

    inline const register_info& get_register_info_by_dwarf(std::int32_t dwarf_id) {
        return get_register_info_by([dwarf_id](auto &i) {return i.dwarf_id == dwarf_id;});
    }

}

#endif