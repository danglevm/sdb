#include <unistd.h>
#include <fcntl.h>
#include <libsdb/pipe.hpp>
#include <libsdb/error.hpp>
#include <utility>

sdb::pipe::pipe(bool close_on_exec) {
    /* pipe2 functions like pipe but with a second file descriptor for I/O mechanism */
    if (pipe2(fds_, close_on_exec ? O_CLOEXEC : 0) < 0) {
        error::send_errno("Pipe creation failed");
    }
}

sdb::pipe::~pipe() {
    close_read();
    close_write();
}

int sdb::pipe::release_read() {
    return std::exchange(fds_[read_fd], -1);
}