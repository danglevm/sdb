#include <string>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <libsdb/process.hpp>
#include <libsdb/pipe.hpp>
#include <libsdb/bits.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libsdb/error.hpp>
#include <fstream>
#include <iostream>

using namespace sdb;
namespace {
    bool process_exists(pid_t pid) {
        auto ret = kill(pid, 0);
        return (ret != -1 and errno != ESRCH);
    };

    char get_process_status(pid_t pid) {
        /* open process file for reading */
        std::ifstream stat("/proc/" + std::to_string(pid) + "/stat");
        std::string data;
        std::getline(stat, data);
        auto index_of_last_parenthesis = data.rfind(')');
        auto index_of_status_indicator = index_of_last_parenthesis + 2;
        return data[index_of_status_indicator];
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
TEST_CASE("Write register rsi", "[register]") {
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

    regs.write_by_id(register_id::ftw, std::uint16_t{0b0011111111111111});
    proc->resume();
    proc->wait_on_signal();

    output = channel.read();
    REQUIRE(to_string_view(output) == "42.24");
}