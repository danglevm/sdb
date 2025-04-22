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
}

sdb::elf::~elf() {
    munmap(file_data_, file_size_);
    close(fd_);
}

/* we use sections during coding so that the linking and debugger understands the process */
void sdb::elf::parse_section_headers() {
    /* resize section_headers_ based on how many available in section header */
    section_headers_.resize(section_headers_.shnum);
    std::copy(data_ + header_.e_shoff, , reinterpret_cast<std::byte*> section_headers);

}