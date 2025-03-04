#include <iostream>
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


/**/
/* anonymous namespace - usage only to the current translation unit or .cpp file */
namespace 
{   
    
    std::unique_ptr<sdb::Process> attach(int argc, const char ** argv)
    { 
        pid_t pid = 0;
        /* Passing PID */
        /* argv[1] == "-p" checks the pointer */
        if (argc == 3 && argv[1] == std::string_view("-p")) 
        {
            pid_t pid = std::atoi(argv[2]);
            return sdb::Process::attach(pid);
        }
        /* passing program's name*/
        else 
        {
            const char * program_path = argv[1];
            return sdb::Process::launch(program_path);
        }
    }
    /* read delimited text from the string */
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

    /* indicates whether a string is equal to a prefix of another string */
    /* used to check for a string whether it's equal or not */
    bool is_prefix(std::string_view prefix, std::string_view str) 
    {   
        if (prefix.size() > str.size())
            return false;
        return std::equal(prefix.begin(), prefix.end(), str.begin());

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
        std::cout << "Process " << process.get_pid() << ' ';

        switch (reason.reason)
        {
            case sdb::process_state::exited:
                std::cout << "exited with status " << static_cast<int>(reason.info);
                break;

            case sdb::process_state::terminated:
                /* sigabbrev returns the abbreviated name of a signal given its number */
                std::cout << "terminated with signal " << sigabbrev_np(reason.info);
                break;

            case sdb::process_state::stopped:
                std::cout << "stopped with signal " << sigabbrev_np(reason.info);
                break;
        }

        std::cout << std::endl;
    }
    

    /* execute command based on input */
    void handle_command(std::unique_ptr<sdb::Process>& process, std::string_view line)
    {
        /* explicit cast to std::string to prevent std::string::reference object */
        auto args = split(line, ' ');
        auto command = args[0];

        std::uint64_t data_64 {0};
        std::uint32_t data_32 {0};
        std::uint16_t data_16 {0};
        std::uint8_t data_8 {0};

        double test = 3.14;

        

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

    try {
        auto process = attach(argc, argv);
        main_loop(process);
    }
    catch (const sdb::error& err) {
        std::cout << err.what() << '\n';
    }

}