#ifndef SDB_REGISTERS_HPP
#define SDB_REGISTERS_HPP


#include <atomic>
#include <cstdint>
#include <variant>
#include <sys/user.h>
#include <libsdb/register_info.hpp>
#include <libsdb/types.hpp>


/* 
*
*   HANDLING REGISTER VALUES
*
*/
namespace sdb {
    class Process;
    /* 
    * Process handles reading, from registers then debugger handles writing
     */
    class registers {
        public:
            /* disable constructors and assignment operators as each process has its own unique set of reigsters */
            registers() = delete;
            registers(const registers&) = delete;
            registers& operator = (const registers&) = delete;
            registers(registers&&) = delete;
            registers& operator=(registers&&) = delete;

            //using value = /**/;
            using value = std::variant<std::int8_t, std::int16_t, std::int32_t, std::int64_t,   
                                        std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t, 
                                        float, double, long double,
                                        sdb::byte64, sdb::byte128>;

            value read(const register_info& info) const;
            void write(const register_info& info, value val);

            template <typename T>
            T read_by_id_as(register_id id) const {
                return std::get<T>(read(get_register_info_by_id(id)));
            }

            void write_by_id(register_id id, value val) {
                write(get_register_info_by_id(id), val);
            }
        private:
            friend Process;
            registers(Process& proc) : proc_(&proc){}


            user data_; /* stores raw bytes and uses the user struct from sys/user.h - register values */
            Process * proc_; /* responsibe for handling */

    };
}

#endif