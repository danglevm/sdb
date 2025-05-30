#include <cstdint>
#include <iostream>
#include <cstddef>
#include <libsdb/types.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/bits.hpp>
#include <libsdb/process.hpp>
#include <libsdb/parse.hpp>
#include <algorithm>
#include <type_traits>

/* function for this source file */
namespace {
    /* widen it to match register size */
    template<typename T>
    sdb::byte128 widen (const sdb::register_info& info, T t) {
        /* must be known at compile time */
        /* floating point value */
        if constexpr (std::is_floating_point_v<T>) {
            if (info.format == sdb::register_format::double_float) {
                return sdb::as_byte128(static_cast<double>(t));
            }

            if (info.format == sdb::register_format::long_double) {
                return sdb::as_byte128(static_cast<long double>(t));
            }   
        }
        /* cast it based on signed integer */
        else if constexpr (std::is_signed_v<T>) {
            if (info.format == sdb::register_format::uint) {
                switch (info.size) {
                    case 2: return sdb::as_byte128(static_cast<std::int16_t>(t));
                    case 4: return sdb::as_byte128(static_cast<std::int32_t>(t));
                    case 8: return sdb::as_byte128(static_cast<std::int64_t>(t));
                }
            }
        }
        /* unsigned value - zero extension */
        return sdb::as_byte128(t);
    }

}

sdb::registers::value sdb::registers::read(const register_info& info) const {
    /* retrieve registers' raw bytes and reinterpret them as std::bytes */
    auto bytes = sdb::as_bytes(data_); 
    //auto offset = info.offset;

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

/*
* @param info register_info struct to write val into
* @param val  value to write into register_info struct
*/
void sdb::registers::write(const register_info& info, value val) {
    /* retrieve registers' raw bytes and reinterpret them as std::bytes */
    auto bytes = sdb::as_bytes(data_); 

    /* val is a variant, so can be any type in variant, tracks the variant */
    std::visit([&] (auto& v) {
        /* check if the type we are trying to write to register_info matches */
        if (sizeof(v) <= info.size) {
            auto wide = widen(info, v);
            auto val_bytes = as_bytes(wide); //std::byte
            /* allow writing smaller-sized values into registers safely */
            /* allows copying smaller-sized registers and put it into the offset struct */
            std::copy(val_bytes, val_bytes + info.size, bytes + info.offset);
        } else {
            /* throws an exception */
            std::cerr << "sdb::register::write called with \"mismatched register sizes and values\"";
            std::terminate();
        }
    }, val);

    /* write to all fprs */
    if (info.type == register_type::fpr) {
        proc_->write_fprs(data_.i387);
    }
    else {
        /* write to a single GPR or debug register value */
        /* align to 8-byte boundary */
        auto aligned_offset = info.offset & ~0b111;
        proc_->write_user_area(aligned_offset,
                            from_bytes<std::uint64_t>(bytes + aligned_offset));
    }

}