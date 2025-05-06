#ifndef SDB_TARGET_HPP
#define SDB_TARGET_HPP

#include "breakpoint_site.hpp"
#include "types.hpp"

#include <libsdb/elf.hpp>
#include <libsdb/process.hpp>
#include <memory>

namespace sdb {
    class target {
        public:
            /* make this object unique and non-copyable */
            target() = delete;
            target(const Process&) = delete;
            target& operator=(const Process&) = delete;
            // target(target&&) = delete;
            // target& operator=(target&&) = delete;

            static std::unique_ptr<target> launch(std::filesystem::path path, 
                std::optional<int> stdout_replacement_fd = std::nullopt); 
        
            static std::unique_ptr<target> attach (pid_t pid);
            
            sdb::Process& get_proc() { return *proc_; }
            const sdb::Process& get_proc() const { return *proc_; }

            elf& get_elf() { return *elf_; }
            const elf& get_elf() const { return *elf_; }
        private:
            //calls the constructor 
            target(std::unique_ptr<sdb::Process> proc, std::unique_ptr<sdb::elf> elf) : 
                proc_(std::move(proc)), elf_(std::move(elf)){}
            

            std::unique_ptr<sdb::Process> proc_;
            std::unique_ptr<sdb::elf> elf_;
    };
}

#endif