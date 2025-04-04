#ifndef BREAKPOINT_SITE_HPP
#define BREAKPOINT_SITE_HPP

#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb {
    class Process; //forward declaration

    /* tracks breakpoint state function */
    class breakpoint_site {
        public: 
            /* disable constructors, assignment operators */
            breakpoint_site() = delete;
            breakpoint_site(const breakpoint_site&) = delete;
            breakpoint_site& operator=(const breakpoint_site&) = delete;

            using id_type = std::int32_t; /* unique identifier for breakpoint types */
            id_type id() const {return id_;}
            bool is_enabled() const { return is_enabled_;}
            bool is_internal() const { return is_internal_;}
            bool is_hardware() const {return is_hardware_;}
            virt_addr address () const { return address_;}

            /* enable and disable breakpoints */
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
            friend Process;

            //by default, software breakpoints
            breakpoint_site(Process & proc, virt_addr address, 
                    bool is_internal = false, bool is_hardware = false);

            //internal breakpoints get id = -1
            id_type id_;

            Process * process_;
            virt_addr address_;
            bool is_enabled_;
            std::byte saved_data_; // member to hold data we replace with int3 instruction 
            bool is_internal_; //whether breakpoint is for internal usage
            bool is_hardware_; //software or hardware breakpoint
            int hardware_register_index_ = -1; //tracks the breakpoint index being used
    };
}

#endif
