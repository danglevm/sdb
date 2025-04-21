#ifndef SDB_SYSCALLS_HPP
#define SDB_SYSCALLS_HPP

#include <string_view>

/* conversion between syscall ids to names */
namespace sdb {
    std::string_view syscall_id_to_name(int id);
    int name_to_syscall_id(std::string_view name);
}
#endif