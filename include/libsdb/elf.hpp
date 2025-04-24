#ifndef SDB_ELF_HPP
#define SDB_ELF_HPP

#include "types.hpp"
#include <filesystem>
#include <elf.h>
#include <vector>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <libsdb/types.hpp>
#include <span>
#include <map>


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
            std::size_t size() const { return file_size_; }
            const Elf64_Ehdr& header() const { return header_; }
            virt_addr load_bias() const { return load_bias_; }


            /* Add support for obtaining section name string table */
            /* retrieve section names from .shstrtab
            *  @param index   index to grab the string
            *  Returns a string_view that starts at that given index for the section string table 
            */
            std::string_view get_section_name(std::size_t index) const;

            /* retrieve section names from .strtab or .dynstr
            * @param index  index to grab the string - e.sh_name
            * Returns a string that starts at an index for the general string table */
            std::string get_string(std::size_t index) const;

            /* returns a pointer to the section header with the given name, if it exists */
            std::optional<Elf64_Shdr*> get_section(std::string_view name) const;

            /* get the starting file address of a section by name */
            std::optional<sdb::file_addr> get_section_start_address(std::string_view name) const;

            /* returns a span of bytes with data for that section */
            sdb::span<const std::byte>get_section_contents(std::string_view name) const;


            /*
            * Handle load bias
            */
            void notify_loaded(virt_addr addr) {
                load_bias_ = addr;
            }

            /* Retrieve section to which a given file or virtual address belong */
            const Elf64_Shdr* get_section_containing_address(virt_addr addr) const;
            const Elf64_Shdr* get_section_containing_address(file_addr addr) const;

            /* Returns pointer to all symbols that match that name */
            std::vector<const Elf64_Sym*> get_symbols_by_name(std::string_view name) const;

            /* Retrieve a symbol at a on-disk file address */
            std::optional<const Elf64_Sym*> get_symbol_by_address(file_addr addr) const;

            /* Retrieve a symbol at a virtual address */
            std::optional<const Elf64_Sym*> get_symbol_by_address(virt_addr addr) const;
            
            /* Retrieve a symbol containing address */
            std::optional<const Elf64_Sym*> get_symbol_containing_address(file_addr addr) const;
            std::optional<const Elf64_Sym*> get_symbol_containing_address(virt_addr addr) const;
        private:
            int fd_;
            std::filesystem::path path_;

            /* stores information about ELF file */
            std::size_t file_size_;
            std::byte *file_data_; //entry point to elf data
            Elf64_Ehdr header_;
            std::unordered_map<std::string_view, Elf64_Shdr*> section_map_;

            virt_addr load_bias_;
            
            /* parse section_headers */
            void parse_section_headers();
            
            std::vector<Elf64_Shdr> section_headers_;

            /* map all the section names to section headers*/
            void build_section_map();

            /* parsing symbol table */
            void parse_symbol_table();
            std::vector<Elf64_Sym> symbol_table_;

            /* maps names to multiple potential symbol table entries */
            /* a single name to a range of ELF64 symbols */
            std::unordered_multimap<std::string_view, Elf64_Sym*> 
            symbol_name_map_;

            /* struct with custom comparator function */
            /* lhs is lower address, rhs is higher address */
            struct range_comparator{
                bool operator() (
                    std::pair<file_addr, file_addr> lhs,
                    std::pair<file_addr, file_addr> rhs) const {
                        return lhs.first < rhs.first;
                    }
            };

            void build_symbol_maps();

            /* first is the lower address, second is the higher address */
            /* maps a single address range to a single symbol - relate a symbol with an address range */

            /* takes into account only the start address of the range 
            * so we can do std::map::find with {address, <some arbitrary address>}
            */
            std::map<std::pair<sdb::file_addr, sdb::file_addr>, Elf64_Sym*, range_comparator> 
            symbol_addr_map_;

    };

}

#endif