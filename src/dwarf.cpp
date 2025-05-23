#include <cstdint>
#include <libsdb/dwarf.hpp>
#include <libsdb/elf.hpp>
#include <memory>
#include <libsdb/error.hpp>
#include <unordered_map>


namespace {
    /* skip past DIE attributes dependent on the size of their forms */
    void skip_form(std::uint64_t form) {
        switch (form) {

            /* reduces repetition by grouping common cases together */
            case DW_FORM_flag_present:
                break;
            case DW_FORM_data1:
            case DW_FORM_ref1:
            case DW_FORM_flag:
                pos_ += 1; 
                break;
            case DW_FORM_data2:
            case DW_FORM_ref2:
                pos_ += 2; 
                break;
            case DW_FORM_data4:
            case DW_FORM_ref4:
            case DW_FORM_ref_addr:
            case DW_FORM_sec_offset:
            case DW_FORM_strp:
                pos_ += 4; 
                break;
            case DW_FORM_data8:
            case DW_FORM_addr:
                pos_ += 8; 
                break;
            /* for the cases below parse but don't retrieve the data */
            case DW_FORM_sdata:
                sleb128(); 
                break;
            case DW_FORM_udata:
            case DW_FORM_ref_udata:
                uleb128();  
                break;
            case DW_FORM_block1:
                pos_ += u8();
                break;
            case DW_FORM_block2:
                pos_ += u16();
                break;
            case DW_FORM_block4:
                pos_ += u32();
                break;
            case DW_FORM_block:
            case DW_FORM_exprloc:
                pos_ += uleb128();
                break;
            case DW_FORM_string:
                /* iterate until you hit the null terminator */
                while (!finished() && *pos_ != std::byte(0)) {
                    ++pos_;
                }

                /* go past the null terminator */
                ++pos_;
                break;
            case DW_FORM_indirect:
                skip_form(uleb128());
                break;
            default: sdb::error::send("Unrecognized DWARF form");
        }
    }
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
