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


/* anonymous namespace - usage only to the current translation unit or .cpp file */
namespace 
{   
    
    pid_t attach(int argc, const char ** argv)
    { 
        pid_t pid = 0;
        /* Passing PID */
        /* argv[1] == "-p" checks the pointer */
        if (argc == 3 && argv[1] == std::string_view("-p")) 
        {
            pid = std::atoi (argv[2]);
            if (pid <= 0) 
            {
                std::cerr << "Invalid pid\n";
                return EXIT_FAILURE;
            }

            /* attaches to an existing progress */
            /* can send more ptrace requests to the process */
            if (ptrace(PTRACE_ATTACH, pid, /*addr=*/ nullptr, /*data=*/ nullptr) < 0)
            {
                std::perror("Could not attach");
                return EXIT_FAILURE;
            }
        }
        /* passing program's name*/
        else 
        {
            const char * program_path = argv[1];
            if ((pid = fork()) < 0) 
            {
                std::perror("fork failed");
                return EXIT_FAILURE;
            }

            if (pid == 0) 
            {
                /* running child process */
                /* indicate this process is to be traced by the parent */
                if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) 
                {
                    std::perror("Tracing child process failed");
                    return EXIT_FAILURE;
                }

                /* arguments supplied individually and search for the PATH environment */
                /* a flavor of exec - execute a command that replaces the current process by a command specified */
                /* replaces entirely and no new process is forked, no PID created, blah */
                if (execlp(program_path, program_path, nullptr) < 0) 
                {
                    std::perror("Exec failed");
                    return EXIT_FAILURE;
                }
            } 

        
                /* we are in parent process */
        }

        return pid;
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
    bool is_prefix(std::string_view prefix, std::string_view str) 
    {   
        if (prefix.size() > str.size())
            return false;
        return std::equal(prefix.begin(), prefix.end(), str.begin());

    }

    void resume(pid_t pid)
    {   
        /* get the inferior process to continue running */
        if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) 
        {
            std::cerr << "Couldn't resume\n";
            std::exit(-1); /* causes normal program termination with error code -1 */
        }
    }

    void wait_on_signal(pid_t pid)
    {
        int wait_status;
        auto options = 0;
        if (waitpid(pid, &wait_status, options) < 0)
        {
            std::perror("waitpid failed");
            std::exit(-1);
        }
    }

    void handle_command(pid_t pid, std::string_view line)
    {
        /* explicit cast to std::string to prevent std::string::reference object */
        auto args = split(line, ' ');
        auto command = args[0];

        /* execute the command based on input */
        if (is_prefix(command, "continue")) 
        {
            resume(pid);
            wait_on_signal(pid);
        } 
        else {
            std::cerr << "Unknown command\n";
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

    pid_t pid = attach(argc, argv);
    int wait_status;
    auto options = 0;

    if (waitpid(pid, &wait_status, options) < 0) 
    {
        std::perror("waitpid failed");
    }

    /* reading user input using readline */
    char * line = nullptr;

    /* from readline, if readline reads an EOF market, it returns nullptr */
    while ((line = readline("sdb> ")) != nullptr) 
    {
        std::string line_str;

        /* if the line is empty, we retrieve from history the line */
        if (line == std::string_view("")) 
        {
            free(line);
            if (history_length > 0) 
            {
                line_str = history_list()[history_length - 1]->line;
            }
        }
        else 
        {   
            /* if the line is non-empty, we retrieve the line from readline history */
            line_str = line;
            add_history(line);
            free(line);
        }

        /* handle the command if we receive one */
        if (!line_str.empty()) 
        {
            handle_command(pid, line_str);
        }

    }

}