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

/* destroy the pipe by releasing both file descriptors */
sdb::pipe::~pipe() {
    close_read();
    close_write();
}

/* Release fucntions. Return the current fd and set it to an empty fd -1 */
int sdb::pipe::release_read() {
    return std::exchange(fds_[read_fd], -1);
}

int sdb::pipe::release_write() {
    return std::exchange(fds_[write_fd], -1);
}

void sdb::pipe::close_read() {
    if (fds_[read_fd] != -1) {
        close(fds_[read_fd]);
        fds_[read_fd] = -1;
    }
}

void sdb::pipe::close_write() {
    if (fds_[write_fd] != -1) {
        close(fds_[write_fd]);
        fds_[write_fd] = -1;
    }
}

/* read from a pipe */
std::vector<std::byte> sdb::pipe::read() {
    char buf[1024];
    int chars_read;

    /* something is wrong so we read the errno */
    if ((chars_read = ::read(fds_[read_fd], buf, sizeof(buf))) < 0) {
        error::send_errno("Could not read from byte");
    }

    auto bytes = reinterpret_cast<std::byte*> (buf);
    return std::vector<std::byte>(bytes, bytes + chars_read);
}

void sdb::pipe::write(std::byte * from, std::size_t bytes) {
    if (::write(fds_[write_fd], from, bytes) < 0) {
        error::send_errno("Could not write to pipes");
    }
}