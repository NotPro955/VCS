#include "vcs/commands.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace vcs {

void command_init(int argc, char** argv) {
    if (argc > 3) {
        throw std::runtime_error("Usage: vcs init [directory]");
    }

    const std::filesystem::path path = argc == 3 ? argv[2] : ".";
    Repository::initialize(path);
}

} // namespace vcs
