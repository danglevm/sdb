#include <sys/ptrace.h>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

namespace {
    /* in gdb, the id of breakpoints are local to a session */
    /* since it's cumbersome*/
    auto get_next_id() {
        static sdb::breakpoint_site::id_type id = 0;
        return ++id;
    }
}

sdb::breakpoint_site::breakpoint_site(
    Process& proc, virt_addr address, bool is_internal, bool is_hardware)
    : process_{&proc}, address_{address}, is_enabled_{false},
    saved_data_{}, is_internal_{is_internal}, is_hardware_{is_hardware} {
        id_ = (is_internal_ ? -1 : get_next_id()) ;
} 

/* enable breakpoint site */
void sdb::breakpoint_site::enable() {

    /* breakpoint site already enabled*/
    if (is_enabled_)  {
        return;
    }

    if (is_hardware_) {
        hardware_register_index = process_->set_breakpoint_register(id_, address_);
    } else {
        errno = 0;
        std::uint64_t data = ptrace(PTRACE_PEEKDATA, process_->get_pid(), address_, nullptr);
        if (errno != 0) {
            error::send_errno("Enable breakpoint site failed!");
        }
    
        /* only need the first 8 bits of data so zeroed out the others */
        /* replace the instruction again with this data later on */
        saved_data_ = static_cast<std::byte>(data & 0xff);
    
        std::uint64_t int3 = 0xcc;
    
        /* zero out the first 8 bits of data */
        /* replace the first 8 bits with int3 opcode */
        std::uint64_t data_int3 = ((data & ~0xff) | int3);
    
        if (ptrace(PTRACE_POKEDATA, process_->get_pid(), address_, data_int3) < 0) {
            error::send_errno("Enable breakpoint site failed");
        }
    }



    is_enabled_ = true;
}

// disabling breakpoint 
void sdb::breakpoint_site::disable() {
    
    //already disabled
    if (!is_enabled_) {
        return;
    }

    if (is_hardware_) {
        process_->clear_breakpoint_register(hardware_register_index_);
        hardware_register_index_ = -1;
    } else {
        errno = 0;
        std::uint64_t data = ptrace(PTRACE_PEEKDATA, process_->get_pid(), address_, nullptr);
        if (errno != 0) {
            error::send_errno("Disabling breakpoint site failed");
        }
    
        /* zeroed out the opcode of int3, which is 0xcc and then replace the old instruction back in */
        auto restored_data = ((data & ~0xff) | static_cast<std::uint8_t>(saved_data_));
    
        if (ptrace(PTRACE_POKEDATA, process_->get_pid(), address_, restored_data) < 0) {
            error::send_errno("Disabling breakpoint site failed");
        }
    
    }

    is_enabled_ = false;
}