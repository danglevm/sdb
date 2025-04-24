#include <libsdb/types.hpp>
#include <libsdb/elf.hpp>
#include <cassert>

//got virtual address, get file address
sdb::file_addr sdb::virt_addr::convert_to_file_addr(const elf& obj) const {

    //ensure the calling virtual address is valid
    auto section = obj.get_section_containing_address(*this);
    if (!section) {
        return file_addr{};
    } 

    return sdb::file_addr{addr_ - obj.load_bias().addr() , obj};
}

//got file address, get virtual address
sdb::virt_addr sdb::file_addr::convert_to_virt_addr() const {
    assert(elf_ && "convert_to_virt_addr called on null address");
    //ensure this calling file address is valid
    auto section = elf_->get_section_containing_address(*this);
    if (!section) {
        return sdb::virt_addr{};
    }

    //add load bias to stored address to get real virtual address
    return sdb::virt_addr{elf_->load_bias().addr() + addr_};
}