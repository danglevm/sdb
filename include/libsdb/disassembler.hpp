#ifndef DISASSEMBLER_HPP
#define DISASSEMBLER_HPP

#include <cstddef>
#include <libsdb/process.hpp>
#include <optional>

namespace sdb {
    
    class disassembler {

        /* holds string representation and memory address */
        struct instruction {
            virt_addr address;
            std::string text;
        };
        private:
            Process * proc_;

        public:
            disassembler(Process& proc) : proc_(&proc){}

            /* 
            * Returns a disassembled set of n instructions
            * @param n_instructions number of instructions to disassemble starting from an address
            * @param address        starting address to start disassembling. Default is address at PC
            */
            std::vector<instruction> disassemble(std::size_t n_instructions, 
                    std::optional<virt_addr> address = std::nullopt);
    };
}
#endif