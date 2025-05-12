#include <libsdb/dwarf.hpp>


const std::unordered_map<std::uint64_t, sdb::abbrev>& 
sdb::dwarf::get_abbrev_table(std::size_t offset) {

    if (!abbrev_tables_.count(offset)) {
        //add t nonexistent entry at the requested offset
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));

    }
    return abbrev_tables_.at(offset);
}