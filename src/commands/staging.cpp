#include "vcs/commands.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>

namespace vcs {
namespace {

Bytes read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not read file: " + path.string());
    }
    return Bytes(std::istreambuf_iterator<char>(input), {});
}

std::string write_blob(const Repository& repo, const std::filesystem::path& path) {
    return repo.write_object("blob", read_file(path));
}

} // namespace

void command_add(const Repository& repo, int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("Usage: vcs add <path>...");
    }

    auto index = repo.read_index();
    for (const auto& name : repo.paths_from_args(argc, argv, 2)) {
        const auto path = repo.worktree_path(name);
        if (!std::filesystem::exists(path)) {
            throw std::runtime_error("Path does not exist: " + name);
        }

        if (std::filesystem::is_directory(path)) {
            for (const auto& item : std::filesystem::recursive_directory_iterator(path)) {
                if (item.path().string().rfind(repo.metadata().string(), 0) == 0) {
                    continue;
                }
                if (item.is_regular_file()) {
                    const auto relative = item.path().lexically_relative(repo.worktree());
                    index[relative.generic_string()] = write_blob(repo, item.path());
                }
            }
        } else if (std::filesystem::is_regular_file(path)) {
            index[name] = write_blob(repo, path);
        }
    }
    repo.write_index(index);
}

void command_remove(const Repository& repo, int argc, char** argv) {
    if (argc < 3) {
        throw std::runtime_error("Usage: vcs rm <path>...");
    }

    auto index = repo.read_index();
    for (const auto& name : repo.paths_from_args(argc, argv, 2)) {
        bool found = false;
        for (auto item = index.begin(); item != index.end();) {
            if (item->first == name || item->first.rfind(name + "/", 0) == 0) {
                item = index.erase(item);
                found = true;
            } else {
                ++item;
            }
        }

        if (!found) {
            throw std::runtime_error("Path is not tracked: " + name);
        }
        std::error_code error;
        std::filesystem::remove_all(repo.worktree_path(name), error);
    }
    repo.write_index(index);
}

void command_status(const Repository& repo) {
    const auto index = repo.read_index();
    const auto committed = repo.head_files();
    std::set<std::string> paths;
    for (const auto& [path, _] : index) paths.insert(path);
    for (const auto& [path, _] : committed) paths.insert(path);

    bool clean = true;
    for (const auto& path : paths) {
        const auto staged = index.find(path);
        const auto head = committed.find(path);
        if (head == committed.end()) {
            std::cout << "A  " << path << '\n';
            clean = false;
        } else if (staged == index.end()) {
            std::cout << "D  " << path << '\n';
            clean = false;
        } else if (head->second != staged->second) {
            std::cout << "M  " << path << '\n';
            clean = false;
        }
    }

    for (const auto& [path, _] : index) {
        if (!std::filesystem::exists(repo.worktree_path(path))) {
            std::cout << " D " << path << '\n';
            clean = false;
        }
    }

    for (const auto& item : std::filesystem::recursive_directory_iterator(repo.worktree())) {
        if (item.path().string().rfind(repo.metadata().string(), 0) == 0
            || !item.is_regular_file()) {
            continue;
        }

        const auto path = item.path().lexically_relative(repo.worktree()).generic_string();
        if (!paths.contains(path)) {
            std::cout << "?? " << path << '\n';
            clean = false;
        } else if (index.contains(path)) {
            const auto contents = read_file(item.path());
            // Read the staged blob to compare it with the worktree file.
            const auto staged_id = index.at(path);
            std::string type;
            const auto staged_contents = repo.read_object(staged_id, &type);
            if (type != "blob" || staged_contents != contents) {
                std::cout << " M " << path << '\n';
                clean = false;
            }
        }
    }

    if (clean) {
        std::cout << "Clean worktree\n";
    }
}

} // namespace vcs
