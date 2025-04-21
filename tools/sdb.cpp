#include <iostream>
#include <type_traits>
#include <unistd.h> /* provides access to POSIX API */
#include <string_view>
#include <string>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <readline/readline.h>
#include <readline/history.h>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/register_info.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/parse.hpp>
#include <libsdb/disassembler.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <csignal>

/* anonymous namespace - usage only to the current translation unit or .cpp file */
namespace 
{   
    sdb::Process* g_sdb_process = nullptr;

    /* signal handler that sends a SIGSTOP */
    void handle_sigint(int) {
        kill(g_sdb_process->get_pid(), SIGSTOP);
    }



    std::unique_ptr<sdb::Process> attach(int argc, const char ** argv)
    { 
        pid_t pid = 0;
        /* Passing PID - program is running */
        /* argv[1] == "-p" checks the pointer */
        if (argc == 3 && argv[1] == std::string_view("-p")) 
        {
            pid_t pid = std::atoi(argv[2]);
            return sdb::Process::attach(pid);
        }
        /* passing program's name - program hasn't run yet */
        else 
        {
            const char * program_path = argv[1];
            auto proc = sdb::Process::launch(program_path);
            fmt::print("Launched process with PID {}\n", proc->get_pid());
            return proc;
        }
    }

    /* utility functions */

    /* gets debug information if process is stopped from SIGTRAP */
    std::string get_sigtrap_info(const sdb::Process& process, sdb::stop_reason reason) {
        if (reason.trap_reason == sdb::trap_type::software_break) {
            auto &site = process.breakpoint_sites().get_by_address(process.get_pc()); 
            return fmt::format("(breakpoint {})", site.id());
        }
        if (reason.trap_reason == sdb::trap_type::hardware_break) {
            auto id = process.get_current_hardware_stoppoint();

            //returned index is hardware breakpoint
            if (id.index() == 0) {
                return fmt::format("(breakpoint {})", std::get<0>(id));
            }

            //otherwise it's a watchpoint
            std::string message;

            //index is watchpoint
            auto &point = process.watchpoint_sites().get_by_id(std::get<1>(id));
            message += fmt::format("(watchpoint {})", point.id());

            if (point.previous_data() == point.data()) {
                message += fmt::format("\nValue: {:#x}", point.data());
            } else {
                message += fmt::format("\nOld Value: {:#x} New Value: {:#x}", 
                                        point.previous_data(), point.data());
            }
            return message;
        } else if (reason.trap_reason == sdb::trap_type::single_step) {
            return "(single step)";
        }

        return "";
    }

    /* read delimited text from the string */
    // returns std::vector<std::string> for args variable 
    std::vector<std::string> split(std::string_view str, char delimiter) 
    {
        std::vector<std::string> out{};
        std::stringstream ss {std::string{str}};
        std::string item;

        /* read from a stream and store it into a string item */
        while (std::getline(ss, item, delimiter)) 
        {
            out.push_back(item); /* stores it into the vector */
        }

        return out;
    }

    /* for writing to register value, parse out the values */
    sdb::registers::value parse_register_value(sdb::register_info info, std::string_view text) {
        try {
            if (info.format == sdb::register_format::uint) {
                    switch (info.size) {
                        case 1: return sdb::to_integral<std::uint8_t>(text, 16).value(); //1 byte
                        case 2: return sdb::to_integral<std::uint16_t>(text, 16).value();
                        case 4: return sdb::to_integral<std::uint32_t>(text, 16).value();
                        case 8: return sdb::to_integral<std::uint64_t>(text, 16).value();
                    }
                }
                else if (info.format == sdb::register_format::double_float) {
                    return sdb::to_float<double>(text).value();
                }
                else if (info.format == sdb::register_format::long_double) {
                    return sdb::to_float<long double>(text).value();
                }
                else if (info.format == sdb::register_format::vector) {
                    if (info.size == 8) {
                        return sdb::parse_vector<8>(text);
                    }
                    else if (info.size == 16){
                        return sdb::parse_vector<16>(text);
                    }
                }
            }
        /* catch all exceptions with ... */
        catch(...) {
            sdb::error::send("Invalid format");
        }
    }

    /* indicates whether a string is equal to a prefix of another string */
    /* used to check for a string whether it's equal or not */
    bool is_prefix(std::string_view prefix, std::string_view str) 
    {   
        if (prefix.size() > str.size())
            return false;
        return std::equal(prefix.begin(), prefix.end(), str.begin());
    }


    void print_help(const std::vector<std::string> & args)  {
        if (args.size() == 1) {
            std::cerr << R"(Available commands:
            breakpoint  - Commands for operating on breakpoints
            watchpoint  - Commands for operating on watchpoints
            continue    - Resume the process
            memory      - Commands for operating on memory
            disassemble - Disassemble machine code to assembly
            register    - Commands for operating on registers
            step        - Step over a single instruction
    )";
        
        } else if (is_prefix(args[1], "memory")) {
            std::cerr << R"(Available commands:
            read <address> - default is 32 bytes
            read <address> <number of bytes> 
            write <address> <bytes>
    )";
        /* handles registers */
        } else if (is_prefix(args[1], "register")) {
            std::cerr << R"(Available commands:
            read
            read <register>
            read all
            write <register> <value>
    )" << "\n";
        } else if (is_prefix(args[1], "breakpoint")) {
            std::cerr << R"(Available commands:
            list
            delete  <id>
            disable <id>
            enable  <id>
            set <address>
            set <address> -h
    )";
        } else if (is_prefix(args[1], "watchpoint")) {
            std::cerr << R"(Available commands:
            list
            delete  <id>
            disable <id>
            enable  <id>
            set <address>
            set <address> <write|rw|execute> <size in byte>
    )";
        } else if (is_prefix(args[1], "step")) {
            std::cerr << R"(Available commands:
            breakpoint - Commands for operating on breakpoints
            continue - Resume the process
            register - Commands for operating on registers
            step - Step over a single instruction
    )";
        }
        else if (is_prefix(args[1], "disassemble")) {
            std::cerr << R"(Available options:
            -c <number of instructions>
            -a <starting address>
    )";
        }
        
        else {
            std::cerr << "No help available on that\n";
        }
    }

    void print_disassembly(sdb::Process& process, sdb::virt_addr address,
                            size_t n_instructions) {
        sdb::disassembler dis(process);
        auto instructions = dis.disassemble(n_instructions, address);
        for (auto &instr : instructions) {
            //prints 18 characters wide
            fmt::print("{:#018x}: {} \n", instr.address.addr(), instr.text);
        }

    }

        /* prints the stop reason for the process */
        void print_stop_reason(const sdb::Process& process, sdb::stop_reason reason)
        {
            std::string message;
    
            switch (reason.reason)
            {
                case sdb::process_state::exited:
                    message = fmt::format("exited with status {}", static_cast<int>(reason.info));
                    break;
    
                case sdb::process_state::terminated:
                    /* sigabbrev returns the abbreviated name of a signal given its number */
                    message = fmt::format("terminated with signal {}", sigabbrev_np(reason.info));
                    break;
    
                case sdb::process_state::stopped:
                    message = fmt::format("stopped with signal{} at {:#x}", sigabbrev_np(reason.info), process.get_pc().addr());
                    if (reason.info == SIGTRAP) {
                        message += get_sigtrap_info(process, reason);
                    }
                    break;
            }
    
            fmt::print("Process {} {} \n", process.get_pid(), message);
        }

    void handle_register_read(
        sdb::Process & process,
        const std::vector<std::string>& args) {
            /* checks the type of t then check if it's floating point of integral */
            /* returns std::string */
            auto format = [](auto t) {
                if constexpr (std::is_floating_point_v<decltype(t)>) {
                    return fmt::format("{}", t);
                }
                else if constexpr (std::is_integral_v<decltype(t)>) { 
                    /* enables the amount of padding 0s to use*/
                    return fmt::format("{:#0{}x}", t, sizeof(t) * 2 + 2);
                }
                else {
                    return fmt::format("[{:#04x}]", fmt::join(t, ","));
                }
            };

            /* print all registers or just GPRs */
            if (args.size() == 2 
                or (args.size() == 3 and args[2] == "all")) {
                    
                    //go through every register in the system
                    for (auto& info : sdb::g_register_infos) {
                        auto should_print = (args.size() == 3 or info.type == sdb::register_type::gpr)
                                            and info.name != "orig_rax";
                        if (!should_print) continue;
                        /* if it's a GPR or all, read reg value. */
                        auto val = process.get_registers().read(info);

                        /* prints out information formatting based on type*/
                        fmt::print("{}:\t{}\n", info.name, std::visit(format, val));
                    }
            }
            /* print a single register*/
            else if (args.size() == 3) {
                try {
                    auto info = sdb::get_register_info_by_name(args[2]);
                    auto val = process.get_registers().read(info);
                    /* prints out information formatting based on type */
                    fmt::print("{}:\t{}\n", info.name, std::visit(format, val));
                }
                catch (sdb::error& err) {
                    std::cerr << "No such register\n";
                    return;
                }
            }
            else {
                print_help({"help", "register"});
            }

        }

    /* parser program */
    void handle_register_write(sdb::Process & process, const std::vector<std::string>& args) {
        if (args.size() != 4) {
            print_help({"help", "register"});
            return;
        }

        try {
            auto info = sdb::get_register_info_by_name(args[2]);
            auto value = parse_register_value(info, args[3]);
            process.get_registers().write(info, value);
        }

        catch (sdb::error& err) {
            std::cerr << err.what() << '\n';
            return;
        }

    }

    void handle_register_command(
        sdb::Process &process,
        const std::vector<std::string>& args) {
            if (args.size() < 2){ 
                print_help({"help", "register"});
                return;
            }

            if (is_prefix(args[1], "read")) {
            handle_register_read(process, args);
            }
            else if (is_prefix(args[1], "write")) {
                handle_register_write(process, args);
            }
            else {
                print_help({"help", "register"});
            }
    }

    void handle_breakpoint_command(
        sdb::Process &process,
        const std::vector<std::string>& args) {
            /* list command */
            if (args.size() < 2) {
                print_help({"help", "breakpoint"});
                return;
            }

            auto command = args[1];

            if (is_prefix(command, "list")) {
                if (process.breakpoint_sites().empty()) {
                    fmt::print("No breakpoints set\n");
                }
                else {
                    fmt::print("Current breakpoints:\n");

                    /* each stoppoint from stoppoints_ go into auto& site */
                    /* execute the lambda function for each instance */
                    process.breakpoint_sites().for_each([](auto& site) {
                        if (site.is_internal()) {
                            return;
                        }
                        fmt::print("{}: address = {:#x}, {}, {}\n",
                                    site.id(), site.address().addr(),
                                    site.is_enabled() ? "enabled" : "disabled",
                                    site.is_hardware() ? "hardware" : "software");
                    });
                }
                return;
            }

            /* set breakpoint command */  
            if (args.size() < 3) {
                print_help({"help", "breakpoint"});
                return;
            }

            if (is_prefix(command, "set")) {
                //converts address to 64-bit. returns std::optional
                auto address = sdb::to_integral<std::uint64_t>(args[2], 16);

                if (!address) {
                    fmt::print(stderr,
                        "Breakpoint command expects address in "
                        "hexadecimal, prefixed with '0x'\n");
                    return;
                }
                bool hardware = false;
                if (args.size() == 4) {
                    if(args[3] == "-h") {
                        hardware = true;
                    } else {
                        sdb::error::send("Invalid breakpoint command argument");
                    }
                }

                process.create_breakpoint_site(sdb::virt_addr{*address}, hardware).enable();
                return;
            }

            auto id = sdb::to_integral<sdb::breakpoint_site::id_type>(args[2]);
            if (!id) {
                std::cerr << "Command expects breakpoint id";
                return;
            }

            if (is_prefix(command, "enable")) {
                process.breakpoint_sites().get_by_id(*id).enable();
            }

            else if (is_prefix(command, "disable")) {
                process.breakpoint_sites().get_by_id(*id).disable();
            }

            else if (is_prefix(command, "delete")) {
                process.breakpoint_sites().remove_by_id(*id);
            }
    }

    void handle_memory_read_command(
        sdb::Process& process,
        const std::vector<std::string>& args
    ) {
        //cannot convert memory address
        auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
        if (!address) {
            sdb::error::send("Invalid address format");
        }

        /* default bytes to be read if user doesn't specify */
        auto n_bytes = 32;
        
        //if they have 4 valid arguments - including te number of bytes read
        if (args.size() == 4) {
            auto bytes_arg = sdb::to_integral<std::size_t>(args[3]);
            if (!bytes_arg) {
                sdb::error::send("Invalid number of bytes");
            }
            n_bytes = *bytes_arg;
        }

        auto data = process.read_memory(sdb::virt_addr{*address}, n_bytes);
        //batches up the memory and use fmt::print to write in desired format
        //loops over the data 16 bytes at a time
        for (std::size_t i = 0; i < data.size(); i += 16) { 
            auto start = data.begin() + i;
            auto end = data.begin() + std::min(i + 16, data.size()); //ensures that this doesn't go past the end of the data

            //#016x - prints the address in hex with leading 0x and pads it to 16 characters
            //{:02x} - pad it to two characters
            fmt::print("{:#016x}: {:02x}\n",
                        *address + i, fmt::join(start, end, " "));
        }
    }

    void handle_memory_write_command
    (sdb::Process& process,
     const std::vector<std::string>& args) {

        //write to memory address with what info
        if (args.size() != 4) {
            print_help({"help", "memory"});
            return;
        }

        auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
        if (!address) {
            sdb::error::send("Invalid address format");
        }

        auto data = sdb::parse_vector(args[3]);
        process.write_memory(sdb::virt_addr{ *address}, {data.data(), data.size()});
    }   

    /* for handling memory commands and calls */
    void handle_memory_command(
        sdb::Process& process,
        const std::vector<std::string>& args) {
            if (args.size() < 3) {
                print_help({"help", "memory"});
                return;
            }
            
            if (is_prefix(args[1], "read")) {
                handle_memory_read_command(process, args);
            }

            else if (is_prefix(args[1], "write")) {
                handle_memory_write_command(process, args);
            }
            
            else {
                print_help({"help", "memory"});
            }
        }

    void handle_disassemble_command(sdb::Process& process,
        const std::vector<std::string>& args) {
            auto address = process.get_pc();
            std::size_t n_instructions = 5;

            //get the second argument
            auto it = args.begin() + 1;

            if (*it == "-c" and it + 1 != args.end()) {

                //get number at next arg
                ++it;

                //increment after converting this to integer
                auto ret = sdb::to_integral<std::size_t>(*it++);
                if (!ret) {
                    sdb::error::send("Invalid instruction count");
                }
                n_instructions = *ret;
            } else if (*it == "-a" and it + 1 != args.end()) {
                //get address at next arg
                ++it;

                //increment after converting to integer
                auto ret = sdb::to_integral<std::uint64_t>(*it++, 16);
                if (!ret) {
                    sdb::error::send("Invalid address format");
                }
                address = sdb::virt_addr{*ret};
            } else {
                print_help({"help", "disassemble"});
                return;
            }

            print_disassembly(process, address, n_instructions);
        }
    // void resume(pid_t pid)
    // {   
    //     /* get the inferior process to continue running */
    //     if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) 
    //     {
    //         std::cerr << "Couldn't resume\n";
    //         std::exit(-1); /* causes normal program termination with error code -1 */
    //     }
    // }

    // void wait_on_signal(pid_t pid)
    // {
    //     int wait_status;
    //     auto options = 0;
    //     if (waitpid(pid, &wait_status, options) < 0)
    //     {
    //         std::perror("waitpid failed");
    //         std::exit(-1);
    //     }
    // }

    /*********************
    * Breakpoint commands
    **********************/
    void handle_watchpoint_list(sdb::Process & process, 
        const std::vector<std::string>& args) {
            /* outputs string based on stuffs */
            auto mode_to_string = [] (auto mode) {
                switch(mode) {
                    case sdb::stoppoint_mode::write: return "write";
                    case sdb::stoppoint_mode::read_write: return "read_write";
                    case sdb::stoppoint_mode::execution: return "execution";
                    default: sdb::error::send("Invalid stoppoint mode");
                } 
            };

            if (process.watchpoint_sites().empty()) {
                fmt::print("No watchpoints set\n");
            }
            else {
                fmt::print("Current watchpoints:\n");

                /* each stoppoint from stoppoints_ go into auto& site */
                /* execute the lambda function for each instance */
                process.watchpoint_sites().for_each([&](auto& point) {
                    fmt::print("{}: address = {:#x}, mode = {}, size = {}, {}\n",
                                point.id(), point.address().addr(),
                                mode_to_string(point.mode()), point.size(),
                                point.is_enabled() ? "enabled" : "disabled");
                });
            }
            return;
    }

    void handle_watchpoint_set(sdb::Process & process, 
        const std::vector<std::string>& args) {

            if (args.size() != 5) {
                print_help({ "help", "watchpoint" });
                return;
            }

            auto address = sdb::to_integral<std::uint64_t>(args[2], 16);
            auto mode_text = args[3];
            auto size = sdb::to_integral<std::size_t>(args[4], 10);

            if (!address or !size or 
                !(mode_text == "write" or
                  mode_text == "rw"  or 
                  mode_text == "execute")) {
                    print_help({ "help", "watchpoint" });
                    return;
            }

            sdb::stoppoint_mode mode;
            if (mode_text == "write") mode = sdb::stoppoint_mode::write;
            else if (mode_text == "rw") mode = sdb::stoppoint_mode::read_write;
            else if (mode_text == "execute") mode = sdb::stoppoint_mode::execution;

            process.create_watchpoint(
                sdb::virt_addr{*address}, mode, *size).enable();
    }

    void handle_watchpoint_command(sdb::Process & process, 
        const std::vector<std::string>& args) {

            //doesn't fit any commands
            if (args.size() < 2) {
                print_help({"help", "watchpoint"});
                return;
            }

            auto command = args[1];
            if (is_prefix(command, "list")) {
                handle_watchpoint_list(process, args);
                return;
            }

            if (is_prefix(command, "set")) {
                handle_watchpoint_set(process, args);
                return;
            }

            //expects watchpoint id
            if (args.size() < 3) {
                print_help({ "help", "watchpoint" });
                return;
            }

            auto id = sdb::to_integral<sdb::watchpoint_site::id_type>(args[2]);
            if (!id) {
                std::cerr << "Watchpoint command expects watchpoint id\n";
            }

            if (is_prefix(command, "enable")) {
                process.watchpoint_sites().get_by_id(*id).enable();
            }
            else if (is_prefix(command, "disable")) {
                process.watchpoint_sites().get_by_id(*id).disable();
            }
            else if (is_prefix(command, "delete")) {
                process.watchpoint_sites().remove_by_id(*id);
            }

        }
    

    /*** HANDLING SYSCALL CATCHPOINTS ***/
    void handle_syscall_catchpoint_command(sdb::Process& process, const std::vector<std::string> args) {
        sdb::syscall_catch_policy policy = sdb::syscall_catch_policy::catch_all();

        if (args.size() == 3 and args[2] == "none") {
            policy = sdb::syscall_catch_policy::catch_none();

            /* user wants to trace certain syscalls */
        } else if (args.size() >= 3) {
            //returns vector of string
            auto syscalls = split(args[2], ",");
            std::vector<int> to_catch;

            //back inserter enables std::transform to directly put the result from lambda call into the to catch
            std::transform(begin(syscalls), end(syscalls), 
                        std::back_inserter(to_catch), [](auto & syscall)-> int {
                            //if the argument starts with a digit, it's a syscall number, otherwise it's a name
                            return isdigit(syscall[0]) ? sdb::to_integral<int>(syscall).value() :
                                                        sdb::name_to_syscall_id(syscall);
                        });
            policy = sdb::syscall_catch_policy::catch_some(std::move(to_catch));
        }
    }

    void handle_catchpoint_command(sdb::Process& process, const std::vector<std::string>& args) {
        if (args.size() < 2) {
            print_help({"help", "catchpoint"});
            return;
        }

        if (is_prefix(args[1], "syscall")) {
            handle_syscall_catchpoint_command(process, args);
        }
    }



    void handle_stop(sdb::Process& process, sdb::stop_reason& reason) {
        print_stop_reason(process, reason);

        /* process is stopped */
        if (reason.reason == sdb::process_state::stopped) {
            print_disassembly(process, process.get_pc(), 8);
        }
    }
    

    /* execute command based on input */
    void handle_command(std::unique_ptr<sdb::Process>& process, std::string_view line)
    {
        /* explicit cast to std::string to prevent std::string::reference object */
        auto args = split(line, ' ');
        auto command = args[0];

        // std::uint64_t data_64 {0};
        // std::uint32_t data_32 {0};
        // std::uint16_t data_16 {0};
        // std::uint8_t data_8 {0};

        // double test = 3.14;
        /* execute the command based on input */
        if (is_prefix(command, "continue")) 
        {
            /* raw bytes we are reading from so need to convert it to 64 bytes */
            // data = process->get_registers().read_by_id_as<std::uint64_t>(sdb::register_id::rax);
            // process->get_registers().write_by_id(sdb::register_id::rax, data_8);
            process->resume();
            auto reason = process->wait_on_signal();

            handle_stop(*process, reason);
        } 
        else if (is_prefix(command, "help")) {
            print_help(args);
        }
        else if (is_prefix(command, "register")) {
            handle_register_command(*process, args);
        } 
        else if (is_prefix(command, "breakpoint")) {
            handle_breakpoint_command(*process, args);
        }
        else if (is_prefix(command, "step")) {
            auto reason = process->step_instruction();
            handle_stop(*process, reason);
        }
        else if (is_prefix(command, "memory")) {
            handle_memory_command(*process, args);
        }
        else if (is_prefix(command, "disassemble")) {
            handle_disassemble_command(*process, args);
        }
        else if (is_prefix(command, "watchpoint")) {
            handle_watchpoint_command(*process, args);
        } else if (is_prefix(command, "catchpoint")) {
            handle_catchpoint_command(*process, args);
        }

        else {
            std::cerr << "Unknown command\n";
        }
    }


    void main_loop(std::unique_ptr<sdb::Process> & process) 
    {
        /* reading user input using readline */
        char * line = nullptr;

        /* from readline, if readline reads an EOF market, it returns nullptr */
        while ((line = readline("sdb> ")) != nullptr) 
        {
            std::string line_str;

            /* if the line is empty, we retrieve from history the line */
            if (line == std::string_view("")) {
                free(line);

                if (history_length > 0) {
                    line_str = history_list()[history_length - 1]->line;
                }
            }
            else {   
                /* if the line is non-empty, we retrieve the line from readline history */
                /* remember the last line we read into history */
                line_str = line;
                add_history(line);
                free(line);
            }

            /* handle the command if we receive one */
            if (!line_str.empty()) {
                try {
                    handle_command(process, line_str);
                } catch (const sdb::error & err) {
                    std::cout << err.what() << '\n';
                }
            }

        }
        
    }

}

int main(int argc, const char ** argv) 
{
    /* need the PID or program to attach to */
    if (argc == 1)
    {
        std::cerr << "No arguments given\n";
        return EXIT_FAILURE;
    }

    int value = 42;

    try {
        auto process = attach(argc, argv);
        g_sdb_process = process.get();

        if (signal(SIGINT, handle_sigint) ==  SIG_ERR) {
            std::cerr << "Error occurred while setting the signal handler\n";
            return EXIT_FAILURE;
        }

        /* install handle_sigint */
        main_loop(process);
    }
    catch (const sdb::error& err) {
        std::cout << err.what() << '\n';
    }

}