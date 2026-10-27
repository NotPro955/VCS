#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace vcs {

using FileIndex = std::map<std::string, std::string>;
using Bytes = std::string;

class Repository {
public:
    Repository(std::filesystem::path worktree, std::filesystem::path metadata);

    static Repository find(std::filesystem::path start = std::filesystem::current_path());
    static void initialize(const std::filesystem::path& path);

    const std::filesystem::path& worktree() const;
    const std::filesystem::path& metadata() const;

    std::string write_object(const std::string& type, const Bytes& contents) const;
    Bytes read_object(const std::string& id, std::string* type = nullptr) const;

    FileIndex read_index() const;
    void write_index(const FileIndex& entries) const;
    std::string head_id() const;
    FileIndex head_files() const;
    std::string write_tree(const FileIndex& files) const;
    FileIndex read_tree(const std::string& id) const;
    std::string commit_tree(const std::string& id) const;

    std::filesystem::path worktree_path(const std::string& relative) const;
    std::vector<std::string> paths_from_args(int argc, char** argv, int first) const;

private:
    std::filesystem::path worktree_;
    std::filesystem::path metadata_;
};

} // namespace vcs
