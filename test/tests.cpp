#include <fcntl.h>
#include <string>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <libsdb/process.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/bits.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libsdb/error.hpp>
#include <libsdb/register_info.hpp>
#include <libsdb/syscalls.hpp>
#include <fstream>
#include <iostream>
#include <elf.h>
#include <regex>

using namespace sdb;
namespace {
    bool process_exists(pid_t pid) {
        auto ret = kill(pid, 0);
        return (ret != -1 and errno != ESRCH);
    };

    /* temporary measure to get process status and id at /proc/stat */
    char get_process_status(pid_t pid) {
        /* open process file for reading */
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string data;
        std::getline(stat, data);
        auto index_of_last_parenthesis = data.rfind(')');
        auto index_of_status_indicator = index_of_last_parenthesis + 2;
        return data[index_of_status_indicator];
    }

    /* getting load bias for a ELF section - temporary measure */
    std::int64_t get_section_load_bias(
        std::filesystem::path path, Elf64_Addr file_address) {
            auto command = std::string("readelf -WS ") + path.string();
            auto pipe = popen(command.c_str(), "r");

            std::regex text_regex(R"(PROGBITS\s+(\w+)\s+(\w+)\s+(\w+))");
            char* line = nullptr;
            std::size_t len = 0;

            while(getline(&line, &len, pipe) != -1) {
                std::cmatch groups;
                if (std::regex_search(line, groups, text_regex)) {

                    // address, offset, size of the section  
                    auto address = std::stol(groups[1], nullptr, 16);
                    auto offset = std::stol(groups[2], nullptr, 16);

                    //offset of this section in the ELF file
                    auto size = std::stol(groups[3], nullptr, 16);

                    /* check if the address lies at this section of the file address */
                    if (address <= file_address and file_address < (address + size)) {
                        /* clean up and return section load bias */
                        free(line);
                        pclose(pipe);

                        //where the section lies in the ELF file
                        return address - offset;
                    }
                }

                free(line);
                line = nullptr;
            }
            pclose(pipe);
            sdb::error::send("Could not find section load bias");
    }

    std::int64_t get_entry_point_offset(std::filesystem::path path) {
        std::ifstream elf_file(path);

        //ELF header
        Elf64_Ehdr header;
        //read function expects more
        elf_file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
        auto entry_file_address = header.e_entry;
        return entry_file_address - get_section_load_bias(path, entry_file_address);
    }

    virt_addr get_load_address(pid_t pid, std::int64_t offset) {
        std::ifstream maps("/proc/" + std::to_string(pid) + "/maps");

        //get low address range, executable flag, file off 
        std::regex map_regex (R"((\w+)-\w+ ..(.). (\w+))");

        std::string data;
        while (std::getline(maps, data)) {
            std::smatch groups;
            std::regex_search(data, groups, map_regex);
            if (groups[2] == 'x') {
                //memory map file for given PID as std::ifstream
                auto low_range = std::stol(groups[1], nullptr, 16);
                auto file_offset = std::stol(groups[3], nullptr, 16);
                return virt_addr(offset - file_offset + low_range);
            }
        }
        sdb::error::send("Could not find load address");
    }
    
}


/********************************************************** 
* PROCESS ATTACH
********************************************************** */
/* launch without attaching */
/* launch a test process and attach to it */
TEST_CASE("process::attach success", "[process]") {
    auto target = Process::launch("targets/run_endlessly", false); //don't attach to process
    auto proc = Process::attach(target->get_pid());
    REQUIRE(get_process_status(target->get_pid()) == 't');
}

TEST_CASE("process:attach invalid PID", "[process]") {
    REQUIRE_THROWS_AS(Process::attach(0), error);
}


/********************************************************** 
* PROCESS LAUNCH
********************************************************** */
/* Insufficient permissions for PTRACE_TRACEME (launching) */
/* process exists after launching */
TEST_CASE("Process::launch success", "[process]")
{
    auto proc = Process::launch("yes"); /* launches on the command yes and should pass */
    REQUIRE(process_exists(proc->get_pid()));
}

/* Couldn't launch/execute a program */
/* ensures sdb::process throws an sdb::error exception when launching a non-existent program */
TEST_CASE("Process::launch no such program", "[process]")
{
    /* an exception is thrown as a certain type */
    /* fails when an exception is not found */
    /* might be a problem when the child process throws an exception but doesn't send said exception to the parent */
    REQUIRE_THROWS_AS(Process::launch("Boom_test_failure_program"), error);
}


/********************************************************** 
* PROCESS RESUME
********************************************************** */

TEST_CASE("process::resume success", "[process]") {
/* reuse names within test cases */
    {
        auto proc = Process::launch("targets/run_endlessly");
        proc->resume();
        auto status = get_process_status(proc->get_pid());
        auto success = status == 'S' or status == 'R';
        REQUIRE(success);
    }
    {
        auto target = Process::launch("targets/run_endlessly", false); //don't attach to process
        auto proc = Process::attach(target->get_pid());
        proc->resume();
        auto status = get_process_status(proc->get_pid());
        auto success = status == 'S' or status == 'R';
        REQUIRE(success);
    }
}

/* launches the process, resumes it, then checks if terminated process throws an error */
TEST_CASE("process::resume resume already terminated", "[process]")
{
    auto proc = Process::launch("targets/end_immediately");
    proc->resume();
    proc->wait_on_signal();
    REQUIRE_THROWS_AS(proc->resume(), error);
}

/*************************************/
/** REGISTERS READ AND WRITE TESTS */
/*************************************/

TEST_CASE("Write registers", "[register]") {
    bool close_on_exec = false;
    /* set up pipe if it doesn't close on execute */
    sdb::pipe channel(close_on_exec);

    /* launch regs_write and resume it until it hits the first trap */
    auto proc = Process::launch(
        "targets/reg_write", true, channel.get_write()
    );
    /* close the write end from the parent's POV */
    channel.close_write();

    /* to first trap */
    proc->resume();
    proc->wait_on_signal();

    /* write a value to rsi */
    auto& regs = proc->get_registers();
    regs.write_by_id(register_id::rsi, 0xcafecafe);

    /* to second trap */
    proc->resume();
    proc->wait_on_signal();

    auto output = channel.read();
    REQUIRE(to_string_view(output) == "0xcafecafe");

    /* tests mm registers */
    /* check if output matches value */
    regs.write_by_id(register_id::mm0, 0xba5eba11);

    /* to third trap */
    proc->resume();
    proc->wait_on_signal();

    /* check if output matches value */
    output = channel.read();
    REQUIRE(to_string_view(output) == "0xba5eba11");

    /* tests xmm registers */
    /* check if output matches value */
    regs.write_by_id(register_id::xmm0, 42.24);

    /* to fourth trap */
    proc->resume();
    proc->wait_on_signal();

    /* check if output matches value */
    output = channel.read();
    REQUIRE(to_string_view(output) == "42.24");

    /* top of index */
    regs.write_by_id(register_id::st0, 42.24l);
    
    /* set bits 11-13 (TOS) to 7 */
    regs.write_by_id(register_id::fsw, std::uint16_t{0b0011100000000000});

    /* set ST0 to be valid */
    regs.write_by_id(register_id::ftw, std::uint16_t{0b0011111111111111});
    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.24");
}

TEST_CASE("Read registers", "[register]") {
    auto proc = Process::launch("targets/reg_read", true);
    auto& regs = proc->get_registers();

    /* to 1st trap */
    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint64_t>(register_id::r13) == 0xcafecafe);

    /* to 2nd trap */
    proc->resume();
    proc->wait_on_signal();

    REQUIRE(regs.read_by_id_as<std::uint8_t>(register_id::r13b) == 42);

     /* to 3rd trap */
     proc->resume();
     proc->wait_on_signal();
 
     //ull added for unsigned long long
     REQUIRE(regs.read_by_id_as<sdb::byte64>(register_id::mm0) == sdb::as_byte64(0xba5eba11ull));


     /* to 4th trap */
     proc->resume();
     proc->wait_on_signal();
 
     REQUIRE(regs.read_by_id_as<sdb::byte128>(register_id::xmm0) == sdb::as_byte128(64.125));

     proc->resume();
     proc->wait_on_signal();

     //L suffix added for long double
     REQUIRE(regs.read_by_id_as<long double>(register_id::st0) == 64.125L);
}

/**************
* BREAKPOINTS 
***************/
TEST_CASE("Create breakpoint site", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    auto& site = proc->create_breakpoint_site(virt_addr{42});
    /* check if there's a breakpoint at 42 */
    REQUIRE(site.address().addr() == 42);
}

TEST_CASE("Breakpoint site ids increase", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    auto& s1 = proc->create_breakpoint_site(virt_addr{42});
    REQUIRE(s1.address().addr() == 42);

    /* check if breakpoint site ids actually increase */
    auto& s2 = proc->create_breakpoint_site(virt_addr{43});
    REQUIRE(s2.id() == s1.id() + 1);

    auto& s3 = proc->create_breakpoint_site(virt_addr{44});
    REQUIRE(s3.id() == s1.id() + 2);

    auto& s4 = proc->create_breakpoint_site(virt_addr{45});
    REQUIRE(s4.id() == s1.id() + 3);
}

/* need to test with non-const unique ptr and unique pointer */
TEST_CASE("Can find breakpoint site", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    proc->create_breakpoint_site(virt_addr{42});
    proc->create_breakpoint_site(virt_addr{43}); 
    proc->create_breakpoint_site(virt_addr{44});
    proc->create_breakpoint_site(virt_addr{45});

    auto& s1 = proc->breakpoint_sites().get_by_address(virt_addr{44});
    REQUIRE(proc->breakpoint_sites().contains_address(virt_addr{44}));
    REQUIRE(s1.address().addr() == 44);

    auto& cs1 = cproc->breakpoint_sites().get_by_address(virt_addr{44});
    REQUIRE(cproc->breakpoint_sites().contains_address(virt_addr{44}));
    REQUIRE(cs1.address().addr() == 44);

    auto &s2 = proc->breakpoint_sites().get_by_id(s1.id() + 1);
    REQUIRE(proc->breakpoint_sites().contains_id(s1.id() + 1));
    REQUIRE(s2.address().addr() == 45);
    REQUIRE(s2.id() == s1.id() + 1);

    auto& cs2 = proc->breakpoint_sites().get_by_id(cs1.id() + 1);
    REQUIRE(cproc->breakpoint_sites().contains_id(cs1.id() + 1));
    REQUIRE(cs2.id() == cs1.id() + 1);
    REQUIRE(cs2.address().addr() == 45);
}

TEST_CASE("Cannot find breakpoint site", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    const auto& cproc = proc;

    REQUIRE_THROWS_AS(
        proc->breakpoint_sites().get_by_address(virt_addr{44}), error
    );
    REQUIRE_THROWS_AS(proc->breakpoint_sites().get_by_id(44), error);

    REQUIRE_THROWS_AS(
        cproc->breakpoint_sites().get_by_address(virt_addr{44}), error
    );
    REQUIRE_THROWS_AS(cproc->breakpoint_sites().get_by_id(44), error);
}

TEST_CASE("Breakpoint site list size and emptiness", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    const auto& cproc = proc;
    
    /* no breakpoint sites and it is empty */
    REQUIRE(proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 0);
    REQUIRE(cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 0);

    /* 1 breakpoint site */
    proc->create_breakpoint_site(virt_addr{42});
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 1);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 1);

    /* 2 breakpoint sites */
    proc->create_breakpoint_site(virt_addr{43});
    REQUIRE(!proc->breakpoint_sites().empty());
    REQUIRE(proc->breakpoint_sites().size() == 2);
    REQUIRE(!cproc->breakpoint_sites().empty());
    REQUIRE(cproc->breakpoint_sites().size() == 2);

}

TEST_CASE("Can iterate breakpoint sites", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");
    const auto& cproc = proc;
    proc->create_breakpoint_site(virt_addr{42});
    proc->create_breakpoint_site(virt_addr{43});
    proc->create_breakpoint_site(virt_addr{44});
    proc->create_breakpoint_site(virt_addr{45});

    proc->breakpoint_sites().for_each(
        [addr = 42] (auto & site) mutable {
            REQUIRE(site.address().addr() == addr++);
    });

    cproc->breakpoint_sites().for_each(
        [addr = 42] (auto & site) mutable {
            REQUIRE(site.address().addr() == addr++);
    });
}

TEST_CASE("Breakpoint on address works", "[breakpoint]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = Process::launch("targets/hello_sdb", true, channel.get_write());
    channel.close_write();

    auto offset = get_entry_point_offset("targets/hello_sdb");
    auto load_address = get_load_address(proc->get_pid(), offset);
    proc->create_breakpoint_site(load_address).enable();

    proc->resume();
    auto reason = proc->wait_on_signal();

    REQUIRE(reason.reason == process_state::stopped);
    REQUIRE(reason.info == SIGTRAP);
    REQUIRE(proc->get_pc() == load_address);

    proc->resume();
    reason = proc->wait_on_signal();

    REQUIRE(reason.reason == process_state::exited);
    REQUIRE(reason.info == 0);

    auto data = channel.read();
    REQUIRE(to_string_view(data) == "Hello, sdb!\n");
}

TEST_CASE("Remove breakpoint site works", "[breakpoint]") {
    auto proc = Process::launch("targets/run_endlessly");

    auto& site = proc->create_breakpoint_site(virt_addr{42});
    proc->create_breakpoint_site(virt_addr{43});
    REQUIRE(proc->breakpoint_sites().size() == 2);

    proc->breakpoint_sites().remove_by_id(site.id());
    proc->breakpoint_sites().remove_by_address(virt_addr{43});
    REQUIRE(proc->breakpoint_sites().size() == 0);
    REQUIRE(proc->breakpoint_sites().empty());

}

/*****************
***** MEMORY *****
*****************/
TEST_CASE("Reading and writing memory works", "[memory]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = Process::launch("targets/memory", true, channel.get_write());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    //convert std::byte into strongly-typed std::uint64_t
    //read from STDOUT
    auto a_pointer = from_bytes<std::uint64_t>(channel.read().data());
    
    //read 8 bytes
    auto data_vec = proc->read_memory(virt_addr{ a_pointer }, 8);

    //read the data again as std::byte
    auto data = from_bytes<std::uint64_t>(data_vec.data());

    REQUIRE(data == 0xcafecafe);
}

TEST_CASE("Hardware breakpoint evades checkpoints", "[breakpoint]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = Process::launch("targets/anti_debugger", true, channel.get_write());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    //read an_innocent_function address from the pipe
    auto func = virt_addr(from_bytes<std::uint64_t>(channel.read().data()));

    //software breakpoint with this address
    auto &software_site = proc->create_breakpoint_site(func, false);
    software_site.enable();

    proc->resume();
    proc->wait_on_signal();


    //software breakpoint gets outmaneuvered
    REQUIRE(to_string_view(channel.read()) == "Ultra bamboozled bratan!\n");

    proc->breakpoint_sites().remove_by_id(software_site.id());
    auto &hardware_site = proc->create_breakpoint_site(func, true, false);
    hardware_site.enable();

    proc->resume();
    proc->wait_on_signal();

    //hardware stays right at func, nowhere else
    REQUIRE(proc->get_pc() == func);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(to_string_view(channel.read()) == "You just got bamboozled! You bimbo\n");
}

TEST_CASE("Watchpoint detects reads", "[watchpoint]") {
    bool close_on_exec = false;
    sdb::pipe channel(close_on_exec);

    auto proc = Process::launch("targets/anti_debugger", true, channel.get_write());
    channel.close_write();

    proc->resume();
    proc->wait_on_signal();

    //read an_innocent_function address from the pipe
    auto func = virt_addr(from_bytes<std::uint64_t>(channel.read().data()));

    auto &watch = proc->create_watchpoint(func, sdb::stoppoint_mode::read_write , 1);
    watch.enable();

    proc->resume();
    proc->wait_on_signal();

    proc->step_instruction();
    auto& soft = proc->create_breakpoint_site(func, false);
    soft.enable();

    proc->resume();
    auto reason = proc->wait_on_signal();

    REQUIRE(reason.info == SIGTRAP);

    proc->resume();
    proc->wait_on_signal();

    REQUIRE(to_string_view(channel.read()) == "You just got bamboozled! You bimbo\n");
}

TEST_CASE("Syscall mapping works", "[syscall]") {
    REQUIRE(sdb::syscall_id_to_name(0) == "read");
    REQUIRE(sdb::name_to_syscall_id("read") == 0);
    REQUIRE(sdb::syscall_id_to_name(62) == "kill");
    REQUIRE(sdb::name_to_syscall_id("kill") == 62);
}

TEST_CASE("Syscall catchpoint works", "[catchpoint]") {
    /* our program doesn't pollute the rest */
    auto null_fd = open("/dev/null", O_WRONLY);
    auto proc = Process::launch("targets/anti_debugger", true, null_fd);

    auto write_id = sdb::name_to_syscall_id("write");
    auto policy = sdb::syscall_catch_policy::catch_some({write_id});
    proc->set_syscall_catch_policy(policy);

    proc->resume();
    auto reason = proc->wait_on_signal();

    REQUIRE(reason.reason == sdb::process_state::stopped);
    REQUIRE(reason.info == SIGTRAP);
    REQUIRE(reason.trap_reason == sdb::trap_type::syscall);
    REQUIRE(reason.syscall_info->id == write_id);
    REQUIRE(reason.syscall_info->entry == true);

    proc->resume();
    reason = proc->wait_on_signal();

    REQUIRE(reason.reason == sdb::process_state::stopped);
    REQUIRE(reason.info == SIGTRAP);
    REQUIRE(reason.trap_reason == sdb::trap_type::syscall);
    REQUIRE(reason.syscall_info->id == write_id);
    REQUIRE(reason.syscall_info->entry == false);


    close(null_fd);
}