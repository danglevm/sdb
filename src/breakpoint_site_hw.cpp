#include <libsdb/breakpoint_site_hw.hpp>

namespace {
    auto get_next_id() {
        static sdb::breakpoint_site_hw::type_id id = 0;
        return ++id;
    }
}

sdb::breakpoint_site_hw::breakpoint_site_hw(
    Process& proc, virt_addr address)
    : process_{&proc}, address_{address}, is_enabled_{false},
    saved_data_{} {
        id_ = get_next_id();
    } 