#include <fstream>
#include <libsdb/target.hpp>
#include <libsdb/types.hpp>

namespace {
    std::unique_ptr<sdb::elf> create_loaded_elf (const sdb::Process& proc, const std::filesystem::path& path) {
        auto auxv = proc.get_aux_vect();
        auto elf = std::make_unique<sdb::elf>(path);

        //set the load bias by subtracting the real on-disk address from the virtual address
        elf->notify_loaded(sdb::virt_addr{auxv[AT_ENTRY] - elf->header().e_entry});

        return elf;
    }

}

std::unique_ptr<sdb::target> sdb::target::launch(std::filesystem::path path, std::optional<int> stdout_replacement) {
    auto proc = sdb::Process::launch(path, true, stdout_replacement);
    auto ptr = create_loaded_elf(*proc, path);
    return std::unique_ptr<sdb::target>(new target(std::move(proc), std::move(ptr)));
}

std::unique_ptr<sdb::target> sdb::target::attach (pid_t pid) {
    /* use / operator for path concatenation */
    auto elf_path = std::filesystem::path("/proc") / std::to_string(pid) / "/exe";
    auto proc = sdb::Process::attach(pid);
    auto ptr = create_loaded_elf(*proc, elf_path);
    return std::unique_ptr<sdb::target>(new target(std::move(proc), std::move(ptr)));

}