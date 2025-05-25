#include <cstdint>
#include <functional>
#include <libsdb/dwarf.hpp>
#include <libsdb/elf.hpp>
#include <memory>
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

    /* parse a single compile unit from .debug_info */
    std::unique_ptr<sdb::compile_unit> 
    parse_compile_unit(sdb::dwarf& dwarf, const sdb::elf& obj, cursor cur) {
        auto start = cur.get_pos();
        auto size = cur.u32();
        auto version = cur.u16();
        auto offset = cur.u32();
        auto address_size = cur.u8();

        /* we don't support DWARF64 */
        if(size == 0xffffffff) {
            sdb::error::send("sdb only supports DWARF32");
        } 

        if (version != 4) {
            sdb::error::send("sdb only supports DWARFv4");
        }

        if (address_size != 8) {
            sdb::error::send("sdb only supports address size of 8 for DWARF");     
        }

        size += sizeof(uint32_t);  //account for size of compile unit size field
        sdb::span<const std::byte> data = {start, size};
        return std::unique_ptr<sdb::compile_unit>(new sdb::compile_unit(dwarf, data, offset));

    }

    /* store and parse out compile units into vector */
    std::vector<std::unique_ptr<sdb::compile_unit>> 
    parse_compile_units(sdb::dwarf& dwarf, const sdb::elf& obj) {
        cursor cur(obj.get_section_contents(".debug_info"));
        std::vector<std::unique_ptr<sdb::compile_unit>> units;

        while (!cur.finished()) {
            auto unit = parse_compile_unit(dwarf, obj, cur);
            cur += unit->data().size();
            units.push_back(std::move(unit));
        }

        return units;

    }

    /* parse out a single DIE */
    sdb::die parse_die(const sdb::compile_unit& cu, cursor cur) {
        auto pos = cur.get_pos();
        auto abbrev_code = cur.uleb128();

        /* null entry - abbrev code of 0 */
        if (abbrev_code == 0) {
            auto next = cur.get_pos(); //next DIE is at this position
            return sdb::die(next);
        }

        //get abbrev entry associated with this DIE
        auto& abbrev_table = cu.abbrev_table();
        auto& abbrev = abbrev_table.at(abbrev_code);

        /* find location of next DIE and pre-compute */
        std::vector<const std::byte*> attr_locs;
        attr_locs.reserve(abbrev.attr_specs.size());

        /* save location of attributes and skip over current attribute using form info */
        for (auto& attr_spec : abbrev.attr_specs) {
            attr_locs.push_back(cur.get_pos());
            cur.skip_form(attr_spec.form);
        }

        auto next = cur.get_pos();
        return sdb::die(pos, &cu, &abbrev, next, std::move(attr_locs));
    }
}

sdb::dwarf::dwarf(const sdb::elf& parent) : elf_(&parent) {
    compile_units_ = parse_compile_units(*this, parent);
}

const std::unordered_map<std::uint64_t, sdb::abbrev>& 
sdb::dwarf::get_abbrev_table(std::size_t offset) {

    if (!abbrev_tables_.count(offset)) {
        //add non-existent table at the offset
        abbrev_tables_.emplace(offset, parse_abbrev_table(*elf_, offset));

    }
    return abbrev_tables_.at(offset);
}


const std::unordered_map<std::uint64_t, sdb::abbrev>&
sdb::compile_unit::abbrev_table() const {
    return parent_->get_abbrev_table(abbrev_offset_);
}

sdb::die sdb::compile_unit::root() const {
    std::size_t header_size = 11; //compile unit header size
    cursor cur({data_.begin() + header_size, data_.end()});
    return parse_die(*this, cur);
} 

/************
* ITERATORS * 
*************/
sdb::die::children_range::iterator::iterator(const sdb::die& d) {
    cursor next_cur({ d.next_, d.cu_->data().end()});
    die_ = parse_die(*d.cu_, next_cur);
}

bool sdb::die::children_range::iterator::operator==(const iterator &rhs) const {

        /* no DIE storage or stored DIE has abbrev code of 0 means a null iterator */
    auto lhs_null = !die_->abbrev_entry() or !die_.has_value();
    auto rhs_null = !rhs.die_->abbrev_entry() || !rhs.die_.has_value();

    if (lhs_null and rhs_null) {
        return true;
    }

    if (lhs_null or rhs_null) {
        return false;
    }

    return die_->abbrev_entry() == rhs->abbrev_ and 
            die_->next() == rhs->next();


}