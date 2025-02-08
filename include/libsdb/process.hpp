#ifndef SDB_PROCESS_HPP
#define SDB_PROCESS_HPP

#include <filesystem>
#include <memory>
#include <sys/types.h>

/* organize my code inside to avoid conflicts, in this case code for sdb */
namespace sdb 
{
    /* all process states */
    enum class process_state 
    {
        stopped,
        running,
        exited,
        terminated
    };

    struct stop_reason 
    {
        /* a struct has similar characteristics to a class in C++ */
        stop_reason(int wait_status); /* reason for a stop - return value of the xit or the signal that caused the stop/termination */

        process_state reason; /* holds the reason for a stop */
        std::uint8_t info;

    };
    /* sdb::process object */
    class Process 
    {
        public:
            /*********************************************************************
            * factory methods that create and attach the sdb::process object 
            * static: don't need to create an instance of the class to use them 
            *********************************************************************/
            /*
            * Launch a process (run a binary) and attach to it
            * @param path  path to binary or binary file located in $PATH
            * @param debug turn on debug mode for launched process. Launched process waits for attachment
            * @return      process object wrapping stopped child process
            */
            static std::unique_ptr<Process> launch(std::filesystem::path path, bool debug = true);
            
            /*
            * Attach to  running process
            * @param pid running process to attach to
            * @return    process object wrapping stopped process
            */
            static std::unique_ptr<Process> attach (pid_t pid);
            
            /**********************************************************************/
            /* make sure that users can't construct a process object without going through static member functions */
            /* disable default constructor, copy operator, and copy-move operator */
            /*********************************************************************/
            Process() = delete;
            Process(const Process&) = delete;
            Process& operator=(const Process&) = delete;

            /* get methods */
            pid_t get_pid() const { return pid_; }
            process_state get_state() const { return state_; }

            /* class member functions */
            void resume();
            stop_reason wait_on_signal();
            ~Process();

        private:
            pid_t pid_ = 0;
            bool terminate_on_end_ = true; /* track termination */
            process_state state_ = process_state::stopped;
            bool is_attached_ = true;

            /* Process constructor for use by factory methods
            * @param pid              process id 
            * @param terminate_on_end process terminates or not when it's finished. Leave this true for launched process and false if attaching
            * @param is_attached      whether or not to attach to launched process
            */
            Process(pid_t pid, bool terminate_on_end, bool is_attached) : pid_(pid), terminate_on_end_(terminate_on_end), is_attached_(is_attached){}
    };
    

}
#endif