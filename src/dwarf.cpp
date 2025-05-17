#include <cstdint>
#include <libsdb/dwarf.hpp>
#include <libsdb/elf.hpp>
#include <unordered_map>

namespace {
    std::unordered_map<std::uint64_t, sdb::abbrev>
    parse_abbrev_table(const sdb::elf& obj, std::size_t offset) {
        cursor cur(obj.get_section_contents(".debug_abbrev"));
        cur += offset;

        std::unordered_map<std::uint64_t, sdb::abbrev> ret;
        std::uint64_t abbrev_entry_code = 0;
        do {
            auto abbrev_entry_code = cur.uleb128();
            auto tag = cur.uleb128();
            auto has_children = static_cast<bool>(cur.u8());

            std::vector<sdb::attr_spec> attr_specs;
            std::uint64_t attr = 0;
            
            do {
                /* parse abbrevation entries */
                attr = cur.uleb128();
                auto form = cur.uleb128();
                if (attr != 0) {
                    attr_specs.push_back(sdb::attr_spec{attr, form});
                }

            } while (attr != 0);

            /* add entry to abbreviation table */
            if (abbrev_entry_code != 0) {
                ret.emplace(abbrev_entry_code, sdb::abbrev{abbrev_entry_code, tag, has_children, std::move(attr_specs)});
            }

        } while (abbrev_entry_code != 0);

        return ret;

    }
}
const std::unordered_map<std::uint64_t, sdb::abbrev>& 
sdb::dwarf::get_abbrev_table(std::size_t offset) {

    if (!abbrev_tables_.count(offset)) {
        //add t nonexistent entry at the requested offset
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));

    }
    return abbrev_tables_.at(offset);
}