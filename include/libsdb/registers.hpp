#ifndef SDB_REGISTERS_HPP
#define SDB_REGISTERS_HPP

#include "register_info.hpp"
#include <atomic>
#include <cstdint>
#include <variant>
#include <sys/user.h>
#include <libsdb/register_info.hpp>
#include <libsdb/types.hpp>

namespace sdb {
    class process;
    /* 
    * Process handles reading, from registers then debugger handles writing
     */
    class registers {
        public:
            //using value = /**/;
            using value = std::variant<std::int8_t, std::int16_t, std::int32_t, std::int64_t,   
                                        std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, 
                                        float, double, long double,
                                        sdb::byte64, sdb::byte128>;

            value read(const register_info& info) const;
            void write(const register_info& info, value val);

            template <typename T>
            T read_by_id_as(register_id id) const {

            }

            void write_by_id_as(register_id id, value val) const {
                return write(get_register_info_by_id(id), val);
            }
        private:
            registers(process& proc) : proc_(&proc){}
            friend process;

            user data_;
            process * proc_;

            /* disable constructors and assignment operators as each process has its own unique set of reigsters */
            registers()=delete;
            registers(const registers&)=delete;
            registers& operator=(const registers&)=delete;
            registers(registers&&)=delete;
            registers& operator=(registers&&)=delete;
    };
}

#endif