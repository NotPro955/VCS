#include "vcs/commands.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace vcs {

void command_checkout(const Repository& repo, int argc, char** argv) {
    if (argc != 3) {
        throw std::runtime_error("Usage: vcs checkout <branch-or-commit>");
    }

    const std::string name = argv[2];
    const auto branch_ref = repo.metadata() / "refs" / "heads" / name;
    const bool is_branch = std::filesystem::is_regular_file(branch_ref);
    std::string target_id = name;
    if (is_branch) {
        std::ifstream reference(branch_ref);
        std::getline(reference, target_id);
    }

    const auto target_files = repo.read_tree(repo.commit_tree(target_id));
    const auto index = repo.read_index();
    if (index != repo.head_files()) {
        throw std::runtime_error("Checkout refused: staged changes would be lost");
    }

    for (const auto& [path, _] : index) {
        const auto file = repo.worktree_path(path);
        if (!std::filesystem::exists(file)) continue;

        std::ifstream input(file, std::ios::binary);
        const Bytes contents(std::istreambuf_iterator<char>(input), {});
        const auto staged = repo.read_object(index.at(path));
        if (contents != staged) {
            throw std::runtime_error("Checkout refused: local changes in " + path);
        }
    }

    for (const auto& item : std::filesystem::recursive_directory_iterator(repo.worktree())) {
        if (!item.is_regular_file()
            || item.path().string().rfind(repo.metadata().string(), 0) == 0) {
            continue;
        }
        const auto path = item.path().lexically_relative(repo.worktree()).generic_string();
        if (!index.contains(path)) {
            throw std::runtime_error("Checkout would overwrite untracked file: " + path);
        }
    }

    for (const auto& [path, _] : index) {
        if (!target_files.contains(path)) {
            std::error_code error;
            std::filesystem::remove(repo.worktree_path(path), error);
        }
    }

    for (const auto& [path, blob] : target_files) {
        const auto destination = repo.worktree_path(path);
        std::filesystem::create_directories(destination.parent_path());
        const auto contents = repo.read_object(blob);
        std::ofstream output(destination, std::ios::binary);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) {
            throw std::runtime_error("Could not write file: " + path);
        }
    }

    repo.write_index(target_files);
    std::ofstream(repo.metadata() / "HEAD")
        << (is_branch ? "ref: refs/heads/" + name : target_id) << '\n';
}

} // namespace vcs
