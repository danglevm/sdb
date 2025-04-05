#ifndef SDB_WATCHPOINT_HPP
#define SDB_WATCHPOINT_HPP

#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb{
    class Process;

    class watchpoint_site {
        public:

            watchpoint_site() = delete;
            watchpoint_site(const watchpoint_site&) = delete;
            watchpoint_site& operator=(const watchpoint_site&) = delete;
            watchpoint_site(const watchpoint_site&&) = delete;
            watchpoint_site& operator=(const watchpoint_site&&) = delete;

            using id_type = std::int32_t; /* unique identifier for breakpoint types */
            id_type id() const { return id_;}
            bool is_enabled() const { return is_enabled_;}
            bool is_internal() const { return is_internal_;}
            bool is_hardware() const {return is_hardware_;}
            virt_addr address () const { return address_;}

            /* enable and disable watchpoints */
            void enable();
            void disable();

            /* check if breakpoint site is at given addr */
            bool at_address(virt_addr addr) const {
                return address_ == addr;
            }
                        
            /* check if breakpoint site lies in an address range */
            bool in_range(virt_addr low, virt_addr high) const {
                return low <= address_ and high > address_;
            }
        private:
            friend Process;//grants sdb::Process special access

            watchpoint_site(Process & proc, virt_addr address,
                            sdb::stoppoint_mode mode, size_t size);

            id_type id_;
            Process * process_;
            virt_addr address_; //address of watchpoint
            sdb::stoppoint_mode mode_;
            size_t size_;
            bool is_enabled_;
            int hardware_register_index_ = -1; 

    };
}
#endif