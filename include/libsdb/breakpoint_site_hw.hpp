#ifndef BREAKPOINT_SITE_HW_HPP
#define BREAKPOINT_SITE_HW_HPP

#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb {
    class Process; //forward declaration

    /* tracks breakpoint state function */
    class breakpoint_site_hw {
        public: 
            /* disable constructors, assignment operators */
            breakpoint_site_hw() = delete;
            breakpoint_site_hw(const breakpoint_site_hw&) = delete;
            breakpoint_site_hw& operator=(const breakpoint_site_hw&) = delete;

            using id_type = std::int32_t; /* unique identifier for breakpoint types */
            id_type id() const {return id_;}
            bool is_enabled() const { return is_enabled_;}
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
            breakpoint_site_hw(Process & proc, virt_addr address);


            id_type id_;
            Process * process_;
            virt_addr address_;
            bool is_enabled_;
            std::byte saved_data_; /* member to hold data we replace with int3 instruction */
    }
}

#endif
