#include <iostream>
#include <cstddef>
#include <libsdb/types.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/bits.hpp>
#include <libsdb/process.hpp>

sdb::registers::value sdb::registers::read(const register_info& info) const {
    /* retrieve registers' raw bytes and reinterpret them as std::bytes */
    auto bytes = sdb::as_bytes(data_); 

    if (info.format == sdb::register_format::uint) {

        /* handling based on size of register */
        switch(info.size) {
            /* return the interpretation of data fitting the register size */
            case 1: return sdb::from_bytes<std::uint8_t>(bytes + info.offset);
            case 2: return sdb::from_bytes<std::uint16_t>(bytes + info.offset);
            case 4: return sdb::from_bytes<std::uint32_t>(bytes + info.offset);
            case 8: return sdb::from_bytes<std::uint64_t>(bytes + info.offset);
            default: error::send("Unexpected register size");
        }
    } else if (info.format == sdb::register_format::double_float) {
        return sdb::from_bytes<double>(bytes + info.offset);
    } else if (info.format == sdb::register_format::long_double) {
        return sdb::from_bytes<long double>(bytes + info.offset);
    } else if (info.format == sdb::register_format::vector && info.size == 8) {
        return sdb::from_bytes<byte64>(bytes + info.offset);
    } else {
        return sdb::from_bytes<byte128>(bytes + info.offset);
    }
}

sdb::registers::value sdb::registers::write(const register_info& info, value val) {
    /* retrieve registers' raw bytes and reinterpret them as std::bytes */
    auto bytes = sdb::as_bytes(data_); 

    /* val is a variant, so can be any type in variant */
    std::visit([&] (auto& v) {
        /* check if the type we are trying to write to register_info matches */
        if (sizeof(v) == info.size) {
            auto val_bytes = as_bytes(v); //std::byte
            std::copy(val_bytes, val_bytes + sizeof(v), bytes + info.offset);
        } else {
            std::cerr << "sdb::register::write called with ""mistmatched register sizes and values";
            std::terminate();
        }
    }, val);
}