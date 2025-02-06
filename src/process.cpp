#include <cstdlib>
#include <memory>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <filesystem>
#include <libsdb/process.hpp>
#include <libsdb/error.hpp>

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
    return reason;
}
/****************************
* PROCESS METHODS
******************************/

/* launch and attach can call C++ private constructor */
/* handles fed filepath */
std::unique_ptr<sdb::Process>
sdb::Process::launch(std::filesystem::path path) 
{
    pid_t pid;
    if ((pid = fork()) < 0) 
    {
        /* error: fork failed */
        error::send_errno("Process launch failed");
    }

    if (pid == 0) 
    {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) 
        {
            /* error: tracing failed */
            error::send_errno("Tracing failed");
        }
        
        /* gets null-terminated array of characters. C-string */
        if (execlp(path.c_str(), path.c_str(), nullptr) < 0)
        {
            /* error: exec failed */
            /* replaces the current running process with a new process */
            error::send_errno("Exec failed");
        }
    }

    /* terminate_on_end = true as we launched the process */
    //std::unique_ptr<Process> proc = std::make_unique<Process>(pid, /*terminate_on_end=*/true);
    std::unique_ptr<Process> proc (new Process(pid, true));
    proc->wait_on_signal();

    return proc;
}

/* attaching to a running process */
/* handles attaching to a process */
std::unique_ptr<sdb::Process>  
sdb::Process::attach(pid_t pid) 
{
    if (pid == 0) 
    {
        /* error: invalid PID */
        error::send("Invalid PID");
    }

    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0)
    {
        /* Error: Could not attach */
        error::send_errno("Could not attach");
    }

    /* terminate_on_end = false as we don't launch the process */
    // std::unique_ptr<Process> proc = std::make_unique<Process>(pid, /*terminate_on_end=*/false);
    std::unique_ptr<Process> proc (new Process(pid, false));
    proc->wait_on_signal();

    return proc;
}

void sdb::Process::resume() 
{
    /* issue PTRACE_CONT to restart a stopped tracee process */
    if (ptrace(PTRACE_CONT, this->pid_, nullptr, nullptr))
    {
        error::send_errno("Could not resume");
    }
    state_ = process_state::running;
}

/* destroy the process */
sdb::Process::~Process() 
{
    if (pid_ != 0) 
    {
        int status;
        /* the process is running with valid PID */
        if (state_ == process_state::running) 
        {
            /* send it a SIGSTOP and wait for it to stop */
            kill(pid_, SIGSTOP);
            waitpid(pid_, &status, 0);
        }
        /* detach the destructor (the current process or program) from the process */
        ptrace(PTRACE_DETACH, pid_, nullptr, nullptr);
        kill(pid_, SIGCONT);

        /* send it a SIGKILL if we determine that we should destruct it when said program ends*/     
        if (terminate_on_end_) 
        {
            kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
    }
}