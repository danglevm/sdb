#ifndef SDB_PROCESS_HPP
#define SDB_PROCESS_HPP

#include "bits.hpp"
#include "breakpoint_site.hpp"
#include "stoppoint_collection.hpp"
#include <vector>
#include <filesystem>
#include <memory>
#include <sys/types.h>
#include <sys/user.h>
#include <optional>
#include <libsdb/breakpoint_site.hpp>
#include <libsdb/registers.hpp>
#include <libsdb/stoppoint_collection.hpp>
#include <libsdb/types.hpp>
#include <libsdb/bits.hpp>

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
    /* wraps around an inferior/tracee process, storing its PID */
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
            * @param stdout_replacement_fd file descriptor used for stdout
            * @param debug turn on debug mode for launched process. Launched process waits for attachment
            * @return      unique ptr process object wrapping stopped child process
            */
            static std::unique_ptr<Process> launch(std::filesystem::path path, 
                                                    bool debug = true,
                                                    std::optional<int> stdout_replacement_fd = std::nullopt); /* default option is nullptr */
            
            /*
            * Attach to  running process
            * @param pid running process to attach to
            * @return    unique ptr containing process object wrapping stopped process
            */
            static std::unique_ptr<Process> attach (pid_t pid);
            
            /* get methods */
            pid_t get_pid() const { return pid_; }
            process_state get_state() const { return state_; }

            /* class member functions */
            void resume();
            stop_reason wait_on_signal();
            ~Process();

            /* registers handling function */
            void write_user_area(std::size_t offset, std::uint64_t data);
            registers& get_registers() { return *registers_;}
            const registers& get_registers() const { return *registers_;}

            void write_fprs(const user_fpregs_struct& fprs);
            void write_gprs(const user_regs_struct& gprs);

            /* 
            * handling breakpoints 
            */

            /* create breakpoints */
            breakpoint_site& create_breakpoint_site(virt_addr address, bool hardware = false, bool internal = false);

            /* returns the reference, expensive otherwise */
            stoppoint_collection<breakpoint_site>&
            breakpoint_sites() { return breakpoint_sites_; }

            const stoppoint_collection<breakpoint_site>&
            breakpoint_sites() const {return breakpoint_sites_; }

            /* get the program counter */
            virt_addr get_pc () const {
                return virt_addr{
                    get_registers().read_by_id_as<std::uint64_t>(register_id::rip)
                };
            }

            /* writes to rip register */
            void set_pc(virt_addr address) {
                get_registers().write_by_id(register_id::rip, address.addr());
            }

            /* step over machine instruction */
            sdb::stop_reason step_instruction();

            //a virtual address to read from and the number of bytes to read

            /*
            * Read sections of memory from a virtual address of an inferior/tracee process
            * @param address address to read from
            * @param amount  bytes of memory to read from
            */
            std::vector<std::byte> read_memory(sdb::virt_addr address, size_t amount) const;

            std::vector<std::byte> read_memory_without_traps(sdb::virt_addr address, size_t amount) const;

            /*
            * Write to a virtual address with a span of memory
            * @param address virtual address to write to in process
            * @param 
            */
            void write_memory(virt_addr address, span<const std::byte> data);

            /* reads a block of memory as an object with a strong type */
            template<typename T>
            T read_memory_as(virt_addr address) const {
                auto data = read_memory(address, sizeof(T));
                return from_bytes<T>(data.data());
            }

            /* 
            * Set hardware breakpoints and watchpoints
            * @param id         hardware id to set breakpoint at
            * @param address    address to set breakpoint at
            */
            int set_hardware_breakpoint(breakpoint_site::id_type id, virt_addr address); 

            /* 
            * Clear the DR register at the given index
            * @param index  DR register index
            */
            void clear_breakpoint_register(int index);

        private:
            pid_t pid_ = 0; //pid of inferior process
            bool terminate_on_end_ = true; /* track termination */
            process_state state_ = process_state::stopped;
            bool is_attached_ = true;

            /* Process constructor for use by factory methods
            * @param pid              process id 
            * @param terminate_on_end process terminates or not when it's finished. Leave this true for launched process and false if attaching
            * @param is_attached      whether or not to attach to launched process
            */
            Process(pid_t pid, bool terminate_on_end, bool is_attached) : 
                pid_(pid), terminate_on_end_(terminate_on_end), 
                is_attached_(is_attached), registers_(new registers(*this))
            {}

            /* following the rule of three, since we explicitly defined destructor, we need to have copy move and copy operator disabled */
            /**********************************************************************/
            /* make sure that users can't construct a process object without going through static member functions */
            /* disable default constructor, copy operator, and copy-move operator, move constructor and move assignment operator */
            /*********************************************************************/
            Process() = delete;
            Process(const Process&) = delete;
            Process& operator=(const Process&) = delete;
            Process(Process&&) = delete;
            Process& operator=(Process&&) = delete;

            /* handling registers */
            void read_all_registers();
            std::unique_ptr<registers> registers_;

            /* dynamically allocating breakpoint sites and store their info here */
            //std::vector<std::unique_ptr<breakpoint_site>> breakpoint_sites_;
            stoppoint_collection<breakpoint_site> breakpoint_sites_;

            /* 
            * Set hardware breakpoints and watchpoints internally
            * @param address    address to set breakpoint at
            * @param mode       set to execution, reading or read write
            * @param size       size of the hardware breakpoints
            */
            int set_hardware_stoppoint(virt_addr address, sdb::stoppoint_mode mode, std::size_t size);
    };
    

}
#endif