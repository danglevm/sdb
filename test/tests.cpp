#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <libsdb/process.hpp>
#include <catch2/catch_test_macros.hpp>
#include <libsdb/error.hpp>

using namespace sdb;
namespace {
    bool process_exists(pid_t pid) {
        auto ret = kill(pid, 0);
        return (ret != -1 and errno != ESRCH);
    };
}

TEST_CASE("Process::launch success", "[process]")
{
    auto proc = Process::launch("y"); /* launches on the command yes and should pass */
    REQUIRE(process_exists(proc->get_pid()));
}

TEST_CASE("Process::launch no such program", "[process]")
{
    /* an exception is thrown as a certain type */
    /* fails when an exception is not found */
    /* might be a problem when the child process throws an exception but doesn't send said exception to the parent */
    REQUIRE_THROWS_AS(Process::launch("Boom_test_failure_program"), error);
}