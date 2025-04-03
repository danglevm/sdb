#include <libsdb/disassembler.hpp>
#include <Zydis/Zydis.h>


std::vector<sdb::disassembler::instruction> sdb::disassembler::disassemble(std::size_t n_instructions, 
                                                    std::optional<virt_addr> address) {
    //stores this in a ret vector
    std::vector<instruction> ret;
    
    //preallocate space up to n number of instructions
    ret.reserve(n_instructions);

    //if we do not have the address defined
    if(!address) {

        //construct object in place - in this case the int64_t address into a virt_addr
        address.emplace(proc_->get_pc());
    }

    //largest x64 instruction is 15 bytes
    //contains instructions we want to decode
    auto code = proc_->read_memory(*address, 15 * n_instructions);

    ZyanUSize offset = 0;
    ZydisDisassembledInstruction instr; //need the length from this

    //macro to check if the process is successful
    //disassemble an instruction and format it to human-readable format AT&T
    //64-bit mode
    while (ZYAN_SUCCESS(ZydisDisassembleATT(ZYDIS_MACHINE_MODE_LONG_64,
                        address->addr(), code.data() + offset, code.size() - offset, &instr))
                        and n_instructions > 0) {
        
        //add these debugged instructions t
        ret.push_back({*address, instr.text});

        //disassemble a bunch of instructions
        offset += instr.info.length;
        *address += instr.info.length;
        --n_instructions;
    }

    return ret;
}
