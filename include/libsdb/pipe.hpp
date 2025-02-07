#ifndef SDB_PIPE_HPP
#define SDB_PIPE_HPP

#include <vector>
#include <cstddef>

/***********************************
* WRAPPER for raw pipes
************************************/

namespace sdb {
    class pipe {
        public:
            /* prevents implicit conversion to other types */
            explicit pipe(bool close_on_exec);
            ~pipe();

            /* get methods */
            int get_read() const { return fds_[read_fd]; }
            int get_write() const { return fds_[write_fd]; }
            int release_read();
            int release_write();
            void close_read();
            void close_write(); 

            /* r/w operations that operates on raw bytes. */
            std::vector<std::byte> read();
            void write(std::byte * from, std::size_t bytes);


        private:
            static constexpr unsigned read_fd = 0;
            static constexpr unsigned write_fd = 0;
            int fds_[2];
            
    };
}