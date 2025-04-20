#include <libsdb/watchpoint.hpp>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <utility>

namespace {

    //track separately and within each process
    auto get_next_id() {
        static sdb::watchpoint_site::id_type id = 0;
        return ++id;
    }
}

sdb::watchpoint_site::watchpoint_site(Process & proc, virt_addr address,
    sdb::stoppoint_mode mode, size_t size)
    : process_{&proc}, address_{address}, mode_{mode}, size_{size}, is_enabled_{false} {
        if ((address.addr() & (size - 1)) != 0) {
            sdb::error::send("Watchpoint is not aligned to size");
        }

        id_ = get_next_id();
        update_data();
} 

void sdb::watchpoint_site::enable() {
    if (is_enabled_)  {
        return;
    }

    hardware_register_index_ = process_->set_watchpoint(id_, address_, mode_, size_);
    is_enabled_ = true;
}

void sdb::watchpoint_site::disable() {
    if (!is_enabled_) {
        return;
    }

    process_->clear_hardware_stoppoint(hardware_register_index_);
    is_enabled_ = false;
}

void sdb::watchpoint_site::update_data() {
    std::uint64_t new_data = 0;
    auto read = process_->read_memory(address_, size_);
    memcpy(&new_data, read.data(), size_);

    //assigns new_data to data, then returns data and assign to previous_data
    previous_data_ = std::exchange(data_, new_data);
}