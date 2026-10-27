#include "vcs/commands.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace vcs {

int VcsApp::run(int argc, char** argv) const {
    try {
        if (argc < 2) {
            throw std::runtime_error(
                "Usage: vcs <add|branch|checkout|commit|init|log|restore|revert|rm|status>");
        }

        const std::string command = argv[1];
        if (command == "init") {
            command_init(argc, argv);
            return 0;
        }

        const auto repo = Repository::find();
        if (command == "add") {
            command_add(repo, argc, argv);
        } else if (command == "branch") {
            command_branch(repo, argc, argv);
        } else if (command == "checkout") {
            command_checkout(repo, argc, argv);
        } else if (command == "commit") {
            command_commit(repo, argc, argv);
        } else if (command == "log") {
            command_log(repo);
        } else if (command == "rm") {
            command_remove(repo, argc, argv);
        } else if (command == "restore") {
            command_restore(repo, argc, argv);
        } else if (command == "revert") {
            command_revert(repo, argc, argv);
        } else if (command == "status") {
            command_status(repo);
        } else {
            throw std::runtime_error("Unknown command: " + command);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "vcs: " << error.what() << '\n';
        return 1;
    }
}

} // namespace vcs
