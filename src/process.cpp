#include <cstdlib>
#include <memory>
#include <optional>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <filesystem>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/bits.hpp>
#include <cstring>
#include <sys/personality.h>
#include <sys/uio.h>


namespace {
    /* writes the representation of errno to a pipe with given prefix. Send this message and exit with error code */
    /* for child process to call and send it through to the pipe */
    void exit_with_perror(sdb::pipe& channel, std::string const& prefix) {
        auto message = prefix + ": " + std::strerror(errno); 
        channel.write(reinterpret_cast<std::byte*> (message.data()), message.size());
        /* terminate the child process with this exit code */
        std::exit(-1);
    }
}

/* stop reason type methods */
sdb::stop_reason::stop_reason(int wait_status) 
{
    if (WIFEXITED(wait_status))
    {
        /* exit event -> extract exit code */
        reason = process_state::exited;
        info = WEXITSTATUS(wait_status);
    }
    else if (WIFSIGNALED(wait_status))
    {
        /* termination -> extract termination code */
        reason = process_state::terminated;
        info = WTERMSIG(wait_status);
    } 
    else if (WIFSTOPPED(wait_status))
    {
        /* stop -> extract signal codes */
        reason = process_state::stopped;
        info = WSTOPSIG(wait_status);
    }
}

/* checks if the running process has been stopped */
sdb::stop_reason sdb::Process::wait_on_signal()
{
    int wait_status;
    auto options = 0;

    if (waitpid(pid_, &wait_status, options) < 0)
    {
        error::send_errno("waitpid failed");
    }
    stop_reason reason(wait_status);
    state_ = reason.reason;

    /* we have attached to the process and stop it, read all the GPR and FPR into the registers_ variable */
    if (is_attached_ and state_ == process_state::stopped) {
        read_all_registers();


        /* back up one instruction */
        auto instr_begin = get_pc() - 1;

        if (reason.info == SIGTRAP and 
            breakpoint_sites_.enabled_stoppoint_at_address(instr_begin)) {
                set_pc(instr_begin);
            }
    }
    return reason;
}

/* execute one instruciton forward */
sdb::stop_reason sdb::Process::step_instruction() {
    std::optional<sdb::breakpoint_site*> to_reenable;
    auto pc = get_pc();
    if (breakpoint_sites_.enabled_stoppoint_at_address(pc)) {
        auto& bp = breakpoint_sites_.get_by_address(pc);
        bp.disable();
        to_reenable = &bp;
    }
    
    //execute exactly one instruction
    if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0) {
        error::send_errno("Could not single step");
    }

    //wait until the single step has happened.
    auto reason = wait_on_signal();

    if (to_reenable) {
        to_reenable.value()->enable();
    }

    return reason;
}

/****************************
* PROCESS METHODS
******************************/
/* launch and attach can call C++ private constructor */
/* handles fed filepath */
std::unique_ptr<sdb::Process>
sdb::Process::launch(std::filesystem::path path, 
                    bool debug,
                    std::optional<int> stdout_replacement_fd) 
{
    pid_t pid;
    /* we need to create pipes before forking. pipes closed when they are not in use */
    sdb::pipe channel(/*close_on_exec = */true);

    if ((pid = fork()) < 0) 
    {
        /* error: fork failed */
        error::send_errno("Process launch failed");
    }

    /******************************/
    /* CHILD PROCESS */ 
    /****************************** */
    if (pid == 0) 
    {       
        /* disables randomization for current process */
        personality(ADDR_NO_RANDOMIZE);
        /* child process won't be using read end only write end */
        channel.close_read();

        if (stdout_replacement_fd) {
            /* replace stdout fileno with new replacement fileno */
            /* dup2 syscall closes the second fd, duplicates the first fd to the second */
            /* could be a file or a pipe this points to */
            if (dup2(*stdout_replacement_fd, STDOUT_FILENO) < 0) {
                exit_with_perror(channel, "stdout replacement failed");
            }
        }

        if (debug and ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) 
        {
            /* error: tracing failed */
            // error::send_errno("Tracing failed");
            exit_with_perror(channel, "Tracing failed");
        }
        
        /* gets null-terminated array of characters. C-string */
        if (execlp(path.c_str(), path.c_str(), nullptr) < 0)
        {
            /* error: exec failed */
            /* replaces the current running process with a new process */
            // error::send_errno("Exec failed");
            exit_with_perror(channel, "Exec failed");
        }
    }

    /******************************/
    /* PARENT PROCESS */ 
    /****************************** */
    channel.close_write();
    auto data = channel.read();
    channel.close_read();

    /* child process sent an error message, parent process te*/
    /* for debugging */
    if (data.size() > 0) {
        waitpid(pid, nullptr, 0);
        auto chars = reinterpret_cast<char*>(data.data());
        error::send(std::string(chars, chars + data.size()));
    }
    /* terminate_on_end = true as we launched the process */
    //std::unique_ptr<Process> proc = std::make_unique<Process>(pid, /*terminate_on_end=*/true);
    std::unique_ptr<Process> proc (new Process(pid, /*terminate_on_end=*/true, debug));

    if (debug) {
        proc->wait_on_signal();
    }

    return proc;
}

/* attaching to a running process */
/* handles attaching to a process */
std::unique_ptr<sdb::Process>  
sdb::Process::attach(pid_t pid) 
{
    if (pid == 0) 
    {
        /* child process */
        /* error: invalid PID */
        error::send("Error: invalid PID");
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
    {
        /* Error: Could not attach */
        error::send_errno("Could not attach");
    }

    /* terminate_on_end = false as we don't launch the process */
    // std::unique_ptr<Process> proc = std::make_unique<Process>(pid, /*terminate_on_end=*/false);
    std::unique_ptr<Process> proc (new Process(pid, /*terminate_on_end=*/false, /*attached=*/true));
    proc->wait_on_signal();

    return proc;
}

void sdb::Process::resume() 
{
    /* process stopped at breakpoint, step over it */
    auto pc = get_pc();

    /* step over a breakpoint if we are to remove it afterwards */
    if (breakpoint_sites_.enabled_stoppoint_at_address(pc)) {
        auto& bp = breakpoint_sites_.get_by_address(pc);
        //disable and restore the old instruction
        bp.disable();

        //single step over the replace instruction
        if (ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr) < 0) {
            error::send_errno("Failed to single step");
        }

        int wait_status;
        if (waitpid(pid_, &wait_status, 0) < 0) {
            error::send_errno("waitpid failed");
        }

        bp.enable();
    }
    /* restart stopped TRACEE process */
    if (ptrace(PTRACE_CONT, this->pid_, nullptr, nullptr))
    {
        error::send_errno("Could not resume");
    }
    state_ = process_state::running;
}

/* destroy the process object and kill them */
sdb::Process::~Process() 
{
    if (pid_ != 0) 
    {
        int status;
        /* the process is running with valid PID */
        if (is_attached_) {
            if (state_ == process_state::running) 
            {
            /* send it a SIGSTOP and wait for it to stop */
                kill(pid_, SIGSTOP);
                waitpid(pid_, &status, 0);
            }
            /* detach the destructor (the current process or program) from the process */
            ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
            kill(pid_, SIGCONT);
        }

        /* send it a SIGKILL if target process should be destroyed when the program terminates */     
        if (terminate_on_end_) 
        {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}

void sdb::Process::read_all_registers() {
    /* read user_regs_struct into user struct data_ from the process */
    if (ptrace(PTRACE_GETREGS, pid_, nullptr, &get_registers().data_.regs) < 0) {
        error::send("Error: cannot read general purpose registers");
    } 
    /* read user_fpregs_struct into user struct data_ from the process */
    if (ptrace(PTRACE_GETFPREGS, pid_, nullptr, &get_registers().data_.i387) < 0) {
        error::send("Error: cannot read floating point general purpose registers");
    }

    for (int i = 0; i < 8; ++i) {
        //read debug registers
        auto id = static_cast<int>(register_id::dr0) + i; //retrieve ith register from the 0th debug register

        /* calling register_info_by_id using our cast */
        auto info = sdb::get_register_info_by_id(static_cast<register_id> (id));

        errno = 0;
        /* sets errno to signal errors rather than using return value */
        std::int64_t data = ptrace(PTRACE_PEEKUSER, pid_, info.offset, nullptr);
        if (errno != 0) {
            error::send_errno("Error: cannot read debug registers");
        }

        /* write the retrieved data into the corret place in registers_ member */
        get_registers().data_.u_debugreg[i] = data;
    }
}


/*
* HANDLES WRITING REGISTERS
*/
void sdb::Process::write_user_area(std::size_t offset, std::uint64_t data) {
    /* write the given data to the user area at the given offset */
    if (ptrace(PTRACE_POKEUSER, pid_, offset, data) < 0) {
        error::send("Error: cannot write to user area");
    }
}

/* write to all fpregs */
void sdb::Process::write_fprs(const user_fpregs_struct& fprs) {
    if (ptrace(PTRACE_SETFPREGS, pid_, nullptr, &fprs) < 0) {
        error::send_errno("Could not write floating point registers");
    }
}

/* write to all gpregs */
void sdb::Process::write_gprs(const user_regs_struct& gprs) {
    if (ptrace(PTRACE_SETREGS, pid_, nullptr, &gprs) < 0) {
        error::send_errno("Could not write general purpose registers");
    }
}

/* breakpoints */
sdb::breakpoint_site&
sdb::Process::create_breakpoint_site(sdb::virt_addr address)
{
    //disallow two breakpoints pointing to the same site
    if (breakpoint_sites_.contains_address(address)) {
        error::send("Breakpoint site already created at address " + std::to_string(address.addr()));
    }

    return breakpoint_sites_.push(
        std::unique_ptr<breakpoint_site>(new breakpoint_site(*this, address)));
}

std::vector<std::byte> sdb::Process::read_memory(sdb::virt_addr address, size_t amount) const {
    std::vector<std::byte> ret(amount);

    //data read from target process is stored in here
    //filled with struct and size
    iovec local_desc{ ret.data(), ret.size()};

    //from which the memory should be copied - the remote process
    //stores page memories
    std::vector<iovec> remote_descs;

    //populate remote_desc vector with multiple `iovec` entries.
    //until all requested bytes have been accounted for
    while (amount > 0) {
        //how far the offset address goes into page - the second part
        //number bytes left in the current page starting from current offset and in the current page
        auto up_to_next_page = 0x1000 - (address.addr() & 0xfff);

        //read no more than what's left on the page
        auto chunk_size = std::min(amount, up_to_next_page);

        //chunk of memory to 
        remote_descs.push_back({ reinterpret_cast<void*> (address.addr()), chunk_size});

        //how much we have already read
        amount -= chunk_size;
        
        //add how much we have read into
        address += chunk_size;
    }

    if (process_vm_readv(pid_, &local_desc, /* liovcnt=*/1 ,
                         remote_descs.data(), /* riovcnt=*/remote_descs.size(), /* flags= */ 0) < 0) {
        error::send_errno("Error: could not read process memory with process_vm_readv");
    }
    return ret;
}

void sdb::Process::write_memory(virt_addr address, span<const std::byte> data){
    std::size_t written = 0;

    //loop until we use up all data caller gave us
    while (written < data.size()) {
        auto remaining = data.size() - written;
        std::uint64_t word;

        /* full write with 8 bytes */
        if (remaining >= 8) {
            //write the next 8 bytes from the start of the next memory buffer
            word = from_bytes<std::uint64_t>(data.begin() + written);

        /* partial write for writes fewer than 8 bytes */
        } else {
            auto read = read_memory(address + written, 8); //read 8 bytes from that memory address
            auto word_data = reinterpret_cast<char*>(&word); //to use memcpy on this

            //write the first fewer than 8 bytes into the word
            std::memcpy(word_data, data.begin() + written, remaining);

            //the remaining data we write in from what we read
            std::memcpy(word_data + remaining, read.data() + remaining, 8 - remaining);
        }

        if (ptrace(PTRACE_POKEDATA, pid_, address + written, word) < 0) {
            error::send_errno("Failed to write virtual memory");
        }

        //each iteration here writes 8 bytes to the inferior process
        written += 8;
    }
}