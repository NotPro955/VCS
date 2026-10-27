#include "vcs/commands.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace vcs {

void command_commit(const Repository& repo, int argc, char** argv) {
    if (argc < 4 || std::string(argv[2]) != "-m") {
        throw std::runtime_error("Usage: vcs commit -m <message>");
    }

    const auto index = repo.read_index();
    const auto tree = repo.write_tree(index);
    const auto parent = repo.head_id();
    const auto now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    const std::string identity =
        "VCS User <user@localhost> " + std::to_string(now) + " +0000";

    Bytes contents = "tree " + tree + "\n";
    if (!parent.empty()) {
        contents += "parent " + parent + "\n";
    }
    contents += "author " + identity + "\n";
    contents += "committer " + identity + "\n\n";

    for (int i = 3; i < argc; ++i) {
        if (i > 3) contents += ' ';
        contents += argv[i];
    }
    contents += '\n';

    const auto id = repo.write_object("commit", contents);
    std::ifstream head_file(repo.metadata() / "HEAD");
    std::string reference;
    std::getline(head_file, reference);

    if (reference.rfind("ref: ", 0) == 0) {
        const auto target = repo.metadata() / reference.substr(5);
        std::filesystem::create_directories(target.parent_path());
        std::ofstream(target) << id << '\n';
    } else {
        std::ofstream(repo.metadata() / "HEAD") << id << '\n';
    }
    std::cout << '[' << id.substr(0, 7) << "] commit created\n";
}

void command_log(const Repository& repo) {
    auto id = repo.head_id();
    if (id.empty()) {
        throw std::runtime_error("No commits yet");
    }

    while (!id.empty()) {
        std::string type;
        const auto contents = repo.read_object(id, &type);
        if (type != "commit") {
            throw std::runtime_error("Invalid commit history");
        }

        const auto separator = contents.find("\n\n");
        const auto headers = contents.substr(0, separator);
        const auto message = separator == Bytes::npos
            ? std::string{}
            : contents.substr(separator + 2);

        std::cout << "commit " << id << '\n';
        const auto author = headers.find("author ");
        if (author != std::string::npos) {
            const auto end = headers.find('\n', author);
            std::cout << headers.substr(author, end - author) << '\n';
        }
        std::cout << '\n' << message;
        if (message.empty() || message.back() != '\n') {
            std::cout << '\n';
        }
        std::cout << '\n';

        const auto parent = headers.find("parent ");
        if (parent == std::string::npos) {
            break;
        }
        const auto end = headers.find('\n', parent);
        id = headers.substr(parent + 7, end - parent - 7);
    }
}

void command_branch(const Repository& repo, int argc, char** argv) {
    const auto branch_directory = repo.metadata() / "refs" / "heads";
    std::filesystem::create_directories(branch_directory);

    if (argc == 2) {
        std::ifstream head_file(repo.metadata() / "HEAD");
        std::string head;
        std::getline(head_file, head);
        const std::string active = head.rfind("ref: refs/heads/", 0) == 0
            ? head.substr(16)
            : std::string{};

        for (const auto& item : std::filesystem::directory_iterator(branch_directory)) {
            std::ifstream reference(item.path());
            std::string id;
            std::getline(reference, id);
            const auto name = item.path().filename().string();
            std::cout << (name == active ? "* " : "  ") << name << ' ' << id << '\n';
        }
        return;
    }

    if (argc != 3) {
        throw std::runtime_error("Usage: vcs branch [name]");
    }
    const std::string name = argv[2];
    if (name.empty() || name.find('/') != std::string::npos
        || name == "." || name == "..") {
        throw std::runtime_error("Invalid branch name");
    }

    const auto id = repo.head_id();
    if (id.empty()) {
        throw std::runtime_error("Cannot create a branch before the first commit");
    }
    const auto target = branch_directory / name;
    if (std::filesystem::exists(target)) {
        throw std::runtime_error("Branch already exists: " + name);
    }
    std::ofstream(target) << id << '\n';
    std::cout << "Created branch " << name << '\n';
}

} // namespace vcs
