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
#include <fmt/format.h>
#include <fmt/ranges.h>


/**/
/* anonymous namespace - usage only to the current translation unit or .cpp file */
namespace 
{   
    
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
            continue    - Resume the process
            register    - Commands for operating on registers
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
            )";
        } else if (is_prefix(args[1], "step")) {
            std::cerr << R"(Available commands:
            breakpoint - Commands for operating on breakpoints
            continue - Resume the process
            register - Commands for operating on registers
            step - Step over a single instruction
            )";
        }
        
        else {
            std::cerr << "No help available on that\n";
        }
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
                    process.breakpoint_sites().for_each([](auto& site) {
                        fmt::print("{}: address = {:#x}, {}\n",
                                    site.id(), site.address().addr(),
                                    site.is_enabled() ? "enabled" : "disabled");
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

                process.create_breakpoint_site(sdb::virt_addr{address.value()}).enable();
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
                break;
        }

        fmt::print("Process {} {} \n", process.get_pid(), message);
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

            print_stop_reason(*process, reason);
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
            print_stop_reason(*process, reason);
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
    //std::cout << fmt::format("{:#x}", value) << "\n"; // Output: 0x2a

    try {
        auto process = attach(argc, argv);
        main_loop(process);
    }
    catch (const sdb::error& err) {
        std::cout << err.what() << '\n';
    }

}