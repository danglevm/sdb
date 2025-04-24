#include <elf.h>
#include <optional>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <libsdb/error.hpp>
#include <libsdb/bits.hpp>
#include <libsdb/elf.hpp>
#include <cxxabi.h>
#include <algorithm>



sdb::elf::elf(std::filesystem::path& path) {

    this->path_ = path;

    /* allow large files to be opened and read - far larger than off_t size */
    if ((fd_ = open(path_.c_str(), O_LARGEFILE, O_RDONLY)) == -1) {
        sdb::error::send("Cannot open elf file path");
    }

    struct stat stats;
    if (fstat(fd_, &stats) == -1) {
        sdb::error::send("Cannot get elf file stats");
    }

    this->file_size_ = stats.st_size;

    /* map entire ELF file to virtual memory */
    void * ret;
    if ((ret = mmap(0, file_size_, PROT_READ, MAP_SHARED, fd_, 0)) == MAP_FAILED) {
        close(fd_);
        sdb::error::send_errno("Cannot map ELF file data to memory region");
    }

    //file_data_ is in std::byte* now
    this->file_data_ = reinterpret_cast<std::byte*> (ret);

    std::copy(file_data_, file_data_ + sizeof(header_), sdb::as_bytes(header_));

    /* parse the section headers after */
    parse_section_headers();
    parse_symbol_table();
    build_symbol_maps();
}

sdb::elf::~elf() {
    munmap(file_data_, file_size_);
    close(fd_);
}


/* map all the section names to section headers */
/* we use sections during coding so that the linking and debugger understands the process */
void sdb::elf::parse_section_headers() {
    auto num_headers = header_.e_shnum;

    if (num_headers == 0 && header_.e_shentsize != 0) {
        /* there must be more than 0xff00 sections so get the real number of headers */
        num_headers = from_bytes<Elf64_Shdr>(file_data_ + header_.e_shoff).sh_size;
    }

    /* resize section_headers_ based on how many available in section header */
    section_headers_.resize(num_headers);

    // start at the section header table
    std::copy(file_data_ + header_.e_shoff, file_data_ +  header_.e_shoff + sizeof(Elf64_Shdr) * num_headers, 
    reinterpret_cast<std::byte*> (section_headers_.data()));

}

void sdb::elf::parse_symbol_table() {

    auto opt_section = get_section(".symtab");

    /* if it's NULL, try to get it from .dyntab */
    if (!opt_section) {
        opt_section = get_section(".dynsym");
        if (!opt_section) return;
    }    

    auto symtab = *opt_section;
    auto n_entries = (symtab->sh_size) / (symtab->sh_entsize);
    symbol_table_.resize(n_entries);

    std::copy(file_data_ + symtab->sh_offset, file_data_ + symtab->sh_offset + symtab->sh_size,
        reinterpret_cast<std::byte*> (symbol_table_.data()));
}

std::string_view sdb::elf::get_section_name(std::size_t index) const {
    /* grab the section that stores string table for section names - .shstrtab */
    auto& section_header = section_headers_[header_.e_shstrndx]; 

    //returns the string at that given index (a section name), index into the string table
    return {reinterpret_cast<char*>(file_data_) + section_header.sh_offset + index};
} 

/*
* ELF Sections RELATED SECTIONS
*/

void sdb::elf::build_section_map() {
    for (auto & section : section_headers_) {
        section_map_[get_section_name(section.sh_name)] = &section;
    }
}

void sdb::elf::build_symbol_maps() {
    for (auto & symbol : symbol_table_) {
        auto mangled_name = get_string(symbol.st_name);
        int mangled_status;
        auto * demangled_name = abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &mangled_status);
        if (mangled_status == 0) {
            symbol_name_map_.insert({demangled_name, &symbol});
            free(demangled_name);
        }
        symbol_name_map_.insert({mangled_name, &symbol});

        /* if the symbol has an address and a name and not thread-local storage */
        if (symbol.st_value != 0 
            && symbol.st_name !=  0 
            && ELF64_ST_TYPE(symbol.st_info) != STT_TLS) {
                /* add in lower and higher address range */
                auto symbol_addr_range = std::pair(
                    file_addr{symbol.st_value, *this}, file_addr{symbol.st_value + symbol.st_size, *this});
                symbol_addr_map_.insert({symbol_addr_range, &symbol});
        }
    }
}


std::optional<Elf64_Shdr*> sdb::elf::get_section(std::string_view name) const {
    if (section_map_.count(name) != 0) {
        return {section_map_.at(name)};
    }

    return std::nullopt;
}

sdb::span<const std::byte> sdb::elf::get_section_contents(std::string_view name) const {

    //initialize a variable and test its value without having to declare outside of the scope
    if (auto section = get_section(name).value(); section) {
        return {file_data_ + section->sh_offset, std::size_t{section->sh_size}};
    }

    return {nullptr, std::size_t{0}};
}

std::string sdb::elf::get_string(std::size_t index) const {
    /* try getting it from .strtab */
    auto strtab = get_section(".strtab");

    /* if it's NULL, try to get it from .dyntab */
    if (!strtab) {
        strtab = get_section(".dynstr");
        if (!strtab) {
            return "";
        }
    }
    return {reinterpret_cast<char*>(file_data_) + strtab.value()->sh_offset + index};
}

const Elf64_Shdr* sdb::elf::get_section_containing_address(virt_addr addr) const {
    
    for (const auto & section : section_headers_) {
        if (addr >= load_bias_ + section.sh_addr &&  
            addr < load_bias_ +  section.sh_addr + section.sh_size) {
            return &section;
        }
    }
    return nullptr;
}

/* virtual address helper, require section file address + load bias  */
const Elf64_Shdr* sdb::elf::get_section_containing_address(file_addr addr) const {
    /* both ELF file have to refer to the same ELF file */
    if (addr.get_elf_file() != this) {
        return nullptr;
    }
    for (const auto & section : section_headers_) {
        if (section.sh_addr <= addr.addr() &&  
            section.sh_addr + section.sh_size > addr.addr()) {
            return &section;
        }
    }

    return nullptr;
}

std::optional<sdb::file_addr> sdb::elf::get_section_start_address(std::string_view name) const {
    if (auto sect = get_section(name); sect) {
        return file_addr{sect.value()->sh_addr, *this};
    }

    //can't find the section
    return std::nullopt;
}

std::vector<const Elf64_Sym*> sdb::elf::get_symbols_by_name(std::string_view name) const {
    /* retrieve all symbols associated by a name */
    auto [begin, end] = symbol_name_map_.equal_range(name);
    std::vector<const Elf64_Sym*> ret;

    //pass this range and apply a series of changes
    std::transform(begin, end, std::back_inserter(ret),
                    [](auto& pair) { return pair.second;});

    return ret;
}

std::optional<const Elf64_Sym*> sdb::elf::get_symbol_by_address(file_addr addr) const {
    /* if it's not referring to the same ELF file */
    if (addr.get_elf_file() != this) {
        return std::nullopt;
    }
    sdb::file_addr null_addr;

    /* takes into account only the start address of the range 
    * so we can do std::map::find with {address, <some arbitrary address>}
    */
    auto it = symbol_addr_map_.find({addr, null_addr});
    if (it != symbol_addr_map_.end()) {
        return std::nullopt;
    }
    /* we only need the symbol which is the second element */
    return it->second;
}

std::optional<const Elf64_Sym*> sdb::elf::get_symbol_by_address(virt_addr addr) const {
    return sdb::elf::get_symbol_by_address(addr.convert_to_file_addr(*this));
}


std::optional<const Elf64_Sym*> sdb::elf::get_symbol_containing_address(file_addr addr) const {
        /* if it's not referring to the same ELF file */
    if (addr.get_elf_file() != this || symbol_addr_map_.empty()) {
        return std::nullopt;
    }

    sdb::file_addr null_addr;

    /* takes into account only the start address of the range 
    * so we can do std::map::find with {address, <some arbitrary address>}
    */
    auto it = symbol_addr_map_.lower_bound({addr, null_addr});
    
    /* symbol containing that address begins exactly at that address */
    if (it != symbol_addr_map_.end()) {
        auto [key, value] = *it;
        if (key.first == addr) {
            return value;
        }
    }

    /* look at the entry preceding - check if range begins before address and spans beyond it */

    //nothing preceding the beginning entry and it's not the first entry
    if (it == symbol_addr_map_.begin()) {
        return std::nullopt;
    }

    --it;

    //check if the address lies in this preceding range
    if (auto [key, value] = *it; key.first > addr & key.second < addr) {
        return value;
    }

    return std::nullopt;
    
}

std::optional<const Elf64_Sym*> sdb::elf::get_symbol_containing_address(virt_addr address) const {
    return get_symbol_containing_address(address.to_file_addr(*this));
}

