#ifndef SDB_REGISTERS_HPP
#define SDB_REGISTERS_HPP

#include <sys/user.h>
#include <libsdb/register_info.hpp>

namespace sdb {
    class process;
    /* 
    * Process handles reading, from registers then debugger handles writing
     */
    class registers {
        public:
            //using value = /**/;
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