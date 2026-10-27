#include "vcs/commands.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace vcs {
namespace {

Bytes read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Could not read file: " + path.string());
    return Bytes(std::istreambuf_iterator<char>(input), {});
}

void require_clean(const Repository& repo, const FileIndex& head) {
    if (repo.read_index() != head) {
        throw std::runtime_error("Operation refused: staged changes would be lost");
    }
    for (const auto& [path, id] : head) {
        const auto file = repo.worktree_path(path);
        if (!std::filesystem::is_regular_file(file)
            || read_file(file) != repo.read_object(id)) {
            throw std::runtime_error("Operation refused: local changes in " + path);
        }
    }
}

void write_worktree(const Repository& repo,
                    const FileIndex& old_files,
                    const FileIndex& new_files) {
    for (const auto& [path, _] : old_files) {
        if (!new_files.contains(path)) {
            std::error_code error;
            std::filesystem::remove(repo.worktree_path(path), error);
        }
    }
    for (const auto& [path, id] : new_files) {
        const auto destination = repo.worktree_path(path);
        if (!old_files.contains(path) && std::filesystem::exists(destination)) {
            throw std::runtime_error("Operation would overwrite untracked file: " + path);
        }
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination, std::ios::binary);
        const auto contents = repo.read_object(id);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) throw std::runtime_error("Could not write file: " + path);
    }
}

} // namespace

void command_restore(const Repository& repo, int argc, char** argv) {
    if (argc < 3) throw std::runtime_error("Usage: vcs restore <path>...");

    const auto committed = repo.head_files();
    if (committed.empty()) throw std::runtime_error("No committed files to restore");
    for (const auto& name : repo.paths_from_args(argc, argv, 2)) {
        const auto found = committed.find(name);
        if (found == committed.end()) {
            throw std::runtime_error("Path is not present in the current commit: " + name);
        }
        const auto destination = repo.worktree_path(name);
        std::filesystem::create_directories(destination.parent_path());
        std::ofstream output(destination, std::ios::binary);
        const auto contents = repo.read_object(found->second);
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) throw std::runtime_error("Could not write file: " + name);
    }
}

void command_revert(const Repository& repo, int argc, char** argv) {
    if (argc != 3) throw std::runtime_error("Usage: vcs revert <current-commit>");

    const auto current = repo.head_id();
    if (current.empty() || argv[2] != current) {
        throw std::runtime_error("Only the current HEAD commit can be reverted");
    }
    const auto current_files = repo.head_files();
    require_clean(repo, current_files);

    std::string type;
    const auto contents = repo.read_object(current, &type);
    if (type != "commit") throw std::runtime_error("Invalid HEAD commit");
    const auto parent_pos = contents.find("\nparent ");
    if (parent_pos == std::string::npos) {
        throw std::runtime_error("Cannot revert the initial commit");
    }
    const auto parent_start = parent_pos + 8;
    const auto parent_end = contents.find('\n', parent_start);
    const auto parent = contents.substr(parent_start, parent_end - parent_start);
    const auto parent_files = repo.read_tree(repo.commit_tree(parent));

    // Check all collisions before changing any files.
    for (const auto& [path, _] : parent_files) {
        if (!current_files.contains(path)
            && std::filesystem::exists(repo.worktree_path(path))) {
            throw std::runtime_error("Revert would overwrite untracked file: " + path);
        }
    }
    write_worktree(repo, current_files, parent_files);
    repo.write_index(parent_files);

    const auto tree = repo.write_tree(parent_files);
    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    const std::string identity =
        "VCS User <user@localhost> " + std::to_string(now) + " +0000";
    const auto message = "Revert \"" + current.substr(0, 7) + "\"";
    const auto new_commit = repo.write_object("commit",
        "tree " + tree + "\nparent " + current + "\nauthor " + identity
        + "\ncommitter " + identity + "\n\n" + message + "\n");

    std::ifstream head_file(repo.metadata() / "HEAD");
    std::string reference;
    std::getline(head_file, reference);
    if (reference.rfind("ref: ", 0) == 0) {
        const auto target = repo.metadata() / reference.substr(5);
        std::ofstream(target) << new_commit << '\n';
    } else {
        std::ofstream(repo.metadata() / "HEAD") << new_commit << '\n';
    }
    std::cout << '[' << new_commit.substr(0, 7) << "] revert commit created\n";
}

} // namespace vcs
