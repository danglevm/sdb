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
* ITERATOR ** 
*************/
sdb::die::children_range::iterator::iterator(const sdb::die& d) {
    cursor next_cur({ d.next_, d.cu_->data().end()});
    die_ = parse_die(*d.cu_, next_cur);
}

bool sdb::die::children_range::iterator::operator==(const iterator &rhs) const {

    /* no DIE storage or stored DIE (has abbrev code of 0) means a null iterator */
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

sdb::die::children_range::iterator& 
sdb::die::children_range::iterator::operator++() {
    if (!die_->abbrev_entry() or !die_.has_value()) {
        return *this;
    }


    /* next sibiling is next DIE since there's no children */    
    if (!die_->abbrev_entry()->has_children) {
        cursor next_cur({die_->next(), die_->cu()->data().end()});
        die_ = parse_die(*die_->cu(), next_cur);

    } else if (die_->contains(DW_AT_sibling)) {
        /* read DIE sibling */
        die_ = die_.value()[DW_AT_sibling].as_reference(); 

    } else {
        /* iterate over all its children to find the next DIE */
        iterator it(*die_);

        /* as long as they are non-null */
        if (it->abbrev_) ++it;

        cursor next_cur({it->next_, die_->cu()->data().end()});
        die_ = parse_die(*die_->cu(), next_cur);     
    }


    return *this;

}

sdb::die::children_range::iterator
sdb::die::children_range::iterator::operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
}

sdb::die::children_range sdb::die::children() const {
    return children_range(*this);
}


bool sdb::die::contains(std::uint64_t attr) const {
    auto& specs = this->abbrev_->attr_specs;
    auto ret = std::find_if(specs.begin(), specs.end(), [=](auto attr_spec) {
        return attr_spec.attr == attr;
    });

    return ret != specs.end();
}

sdb::attr sdb::die::operator[] (std::uint64_t attr) const {
    auto& specs = this->abbrev_->attr_specs;
    for (std::size_t i = 0; i < specs.size(); i++) {
        if (specs[i].attr == attr) {
            //attr locs ordered similarly to attr specs
            return { cu_, specs[i].attr, specs[i].form, attr_locs_[i]};
        }
    }

    sdb::error::send("Can't find attribute");
}

/************
* ATTRIBUTE * 
*************/

/* we are dealing with x64, so we can read a single 64 bits integer and return that */
sdb::file_addr sdb::attr::as_address() const {
    cursor cur({attr_loc_, cu_->data().end()});
    if (this->form_ != DW_FORM_addr) sdb::error::send("Invalid address type");
    auto elf = this->cu_->parent()->get_elf();
    return sdb::file_addr(cur.u64(), *elf);
}

std::uint32_t sdb::attr::as_section_offset() const {
    cursor cur({attr_loc_, cu_->data().end()});    
    if (this->form_ != DW_FORM_sec_offset) sdb::error::send("Invalid address type");
    return cur.u32();
}

std::uint64_t sdb::attr::as_int() const {
    cursor cur({attr_loc_, cu_->data().end()});  
    switch(form_) {
        case DW_FORM_data1:
            return cur.u8();
        case DW_FORM_data2:
            return cur.u16();
        case DW_FORM_data4:
            return cur.u32();
        case DW_FORM_data8:
            return cur.u64();
        case DW_FORM_udata:
            return cur.uleb128();
        default:
            error::send("Invalid integer type");
    }
}

sdb::span<const std::byte> sdb::attr::as_block() const {
    std::size_t size;
    cursor cur({attr_loc_, cu_->data().end()});
    switch(form_) {
        case DW_FORM_block1:
            size = cur.u8();
            break;
        case DW_FORM_block2:
            size = cur.u16();
            break;
        case DW_FORM_block4:
            size = cur.u32();
            break;
        case DW_FORM_block:
            size = cur.uleb128();
            break;
        default:
            error::send("Invalid block type");
    }

    return {cur.get_pos(), size};
}

sdb::die sdb::attr::as_reference() const {
    cursor cur({attr_loc_, cu_->data().end()});   
    std::size_t offset;

    switch(form_) {
        case DW_FORM_ref1:
            offset = cur.u8();
            break;
        case DW_FORM_ref2:
            offset = cur.u16();
            break;
        case DW_FORM_ref4:
            offset = cur.u32();
            break;
        case DW_FORM_ref8:
            offset = cur.u64();
            break;
        case DW_FORM_ref_udata:
            offset = cur.uleb128();
            break;
        case DW_FORM_ref_addr: {
            offset = cur.u32();
            auto section = this->cu_->parent()->get_elf()->get_section_contents(".debug_info");
            auto die_pos = section.begin() + offset;

            /* find if the DIE we need is in one of*/
            auto& compile_units = this->cu_->parent()->compile_units();
            auto cu_finder = [=](auto& cu) {
                return cu->data().begin() <= die_pos && die_pos < cu->data().end();
            };

            auto cu_for_offset = std::find_if(compile_units.begin(), compile_units.end(), cu_finder);
            cursor ref_cur({die_pos, cu_for_offset->get()->data().end()});
            return parse_die(**cu_for_offset, ref_cur);
        }
        default:
            error::send("Invalid reference type");
    }

    /* read and parse the DIE at that offset */
    cursor ref_cur({cu_->data().begin() + offset, cu_->data().end()});
    return parse_die(*cu_, ref_cur);
}

std::string_view sdb::attr::as_string() const {
    cursor cur({attr_loc_, cu_->data().end()});   
    switch(form_) {
        case DW_FORM_string:
            return cur.string();
        case DW_FORM_strp: {
            /* find the string in .debug_str */
            auto offset_bytes = cur.u32();
            auto string_section = this->cu_->parent()->get_elf()->get_section_contents(".debug_str");
            cursor string_cur({ string_section.begin() + offset_bytes, string_section.end()});
            return string_cur.string();
        }
        default: 
            error::send("Invalid reference type");
    }
}

/* RETRIEVING VALUES at address attributes */
sdb::file_addr sdb::die::low_pc() const {
    return (*this)[DW_AT_low_pc].as_address();
}
sdb::file_addr sdb::die::high_pc() const {
    auto attr = (*this)[DW_AT_high_pc];
    std::uint64_t addr;
    if (attr.form() == DW_FORM_addr) {
        /* high pc as address */
        addr = attr.as_address().addr();
    } else {
        /* high pc as integer offset from low_pc */
        addr = low_pc().addr() + attr.as_int();
    } 
    
    return sdb::file_addr{addr, *cu_->parent()->get_elf()};
}


/* Iterator for range list */
sdb::range_list::iterator::iterator(
    const compile_unit* cu,
    sdb::span<const std::byte> data,
    file_addr base_addr) :
    cu_(cu),
    data_(data),
    base_addr_(base_addr),
    pos_(data.begin()) {
        ++(*this);
    }
