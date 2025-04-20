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

            /* get functions */
            bool is_enabled() const { return is_enabled_;}
            virt_addr address () const { return address_;}
            stoppoint_mode mode() const { return mode_;}
            std::size_t size() const { return size_;}

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

            /* dealing with watchpoint data */
            std::uint64_t data() const { return data_;} 
            std::uint64_t previous_data() const { return previous_data_;} 

            /* re-reads the value at the watched memory location and updates data and previous data  */
            void update_data();
        private:
            friend Process;

            watchpoint_site(Process & proc, virt_addr address,
                            sdb::stoppoint_mode mode, size_t size);

            id_type id_;
            Process * process_;
            virt_addr address_; //address of watchpoint
            sdb::stoppoint_mode mode_;
            size_t size_;
            bool is_enabled_;
            int hardware_register_index_ = -1; 

            std::uint64_t data_ = 0; //current value at read address
            std::uint64_t previous_data_ = 0; //previously read value
    };
}
#endif