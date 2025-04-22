#ifndef SDB_ELF_HPP
#define SDB_ELF_HPP

#include <filesystem>
#include <elf.h>
#include <vector>

/* wrapper class around an ELF file and stores metadata*/
namespace sdb {

    class elf {
        public:
            elf(std::filesystem::path& path);
            ~elf();

            /* elf objects are unique so we delete copy, copy-assignment, move, copy-move */
            elf(const elf&) = delete;
            elf& operator=(const elf& other) = delete;
            elf(const elf&&) = delete;
            elf& operator=(const elf&& other) = delete;

            std::filesystem::path path() const { return path_;}
            const std::size_t size() const { return file_size_; }
            const Elf64_Ehdr& header() const { return header_; }


        private:
            int fd_;
            std::filesystem::path path_;

            /* stores information about ELF file */
            std::size_t file_size_;
            std::byte *file_data_;
            Elf64_Ehdr header_;
            
            /* parse section_headers */
            void parse_section_headers();
            
            std::vector<Elf64_Shdr> section_headers_;


    };

}

#endif