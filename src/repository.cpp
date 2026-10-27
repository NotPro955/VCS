#include "vcs/repository.hpp"

#include <openssl/sha.h>
#include <zlib.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vcs {
namespace fs = std::filesystem;
namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string sha1(const Bytes& data) {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);

    std::ostringstream result;
    result << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        result << std::setw(2) << static_cast<unsigned>(byte);
    }
    return result.str();
}

} // namespace

Repository::Repository(fs::path worktree, fs::path metadata)
    : worktree_(std::move(worktree)), metadata_(std::move(metadata)) {}

Repository Repository::find(fs::path start) {
    auto path = fs::absolute(std::move(start));
    while (true) {
        const auto metadata = path / ".vcs";
        if (fs::is_directory(metadata)) {
            require(path != path.root_path(),
                    "Refusing to use the filesystem root as a VCS worktree");
            return Repository(path, metadata);
        }

        const auto parent = path.parent_path();
        require(parent != path, "Not inside a VCS repository");
        path = parent;
    }
}

void Repository::initialize(const fs::path& path) {
    const auto worktree = fs::absolute(path).lexically_normal();
    require(worktree != worktree.root_path(),
            "Refusing to initialize a VCS repository at the filesystem root");
    fs::create_directories(worktree);
    const auto metadata = worktree / ".vcs";

    require(!fs::exists(metadata) || fs::is_empty(metadata),
            "Repository already exists and is not empty: " + metadata.string());

    fs::create_directories(metadata / "objects");
    fs::create_directories(metadata / "refs" / "heads");
    fs::create_directories(metadata / "refs" / "tags");
    fs::create_directories(metadata / "branches");

    std::ofstream(metadata / "HEAD") << "ref: refs/heads/main\n";
    std::ofstream(metadata / "description") << "VCS repository\n";
    std::ofstream(metadata / "config")
        << "[core]\n"
        << "\trepositoryformatversion = 0\n"
        << "\tfilemode = false\n"
        << "\tbare = false\n";

    std::cout << "Initialized repository in " << metadata << '\n';
}

const fs::path& Repository::worktree() const {
    return worktree_;
}

const fs::path& Repository::metadata() const {
    return metadata_;
}

std::string Repository::write_object(const std::string& type, const Bytes& contents) const {
    const Bytes raw = type + " " + std::to_string(contents.size()) + '\0' + contents;
    const auto id = sha1(raw);

    uLongf compressed_size = compressBound(raw.size());
    Bytes compressed(compressed_size, '\0');
    const auto result = compress2(
        reinterpret_cast<Bytef*>(compressed.data()),
        &compressed_size,
        reinterpret_cast<const Bytef*>(raw.data()),
        raw.size(),
        Z_DEFAULT_COMPRESSION);
    require(result == Z_OK, "Could not compress object");
    compressed.resize(compressed_size);

    const auto file = metadata_ / "objects" / id.substr(0, 2) / id.substr(2);
    fs::create_directories(file.parent_path());
    std::ofstream output(file, std::ios::binary);
    require(static_cast<bool>(output), "Could not write object: " + id);
    output.write(compressed.data(), static_cast<std::streamsize>(compressed.size()));
    return id;
}

Bytes Repository::read_object(const std::string& id, std::string* type) const {
    require(id.size() == 40, "Invalid object id");

    const auto file = metadata_ / "objects" / id.substr(0, 2) / id.substr(2);
    std::ifstream input(file, std::ios::binary);
    require(static_cast<bool>(input), "Object not found: " + id);

    const Bytes compressed((std::istreambuf_iterator<char>(input)), {});
    Bytes raw(std::max<size_t>(4096, compressed.size() * 8), '\0');
    uLongf raw_size = raw.size();

    int result;
    do {
        result = uncompress(
            reinterpret_cast<Bytef*>(raw.data()),
            &raw_size,
            reinterpret_cast<const Bytef*>(compressed.data()),
            compressed.size());
        if (result == Z_BUF_ERROR) {
            raw.resize(raw.size() * 2);
            raw_size = raw.size();
        }
    } while (result == Z_BUF_ERROR);

    require(result == Z_OK, "Corrupt object: " + id);
    raw.resize(raw_size);

    const auto null_byte = raw.find('\0');
    const auto space = raw.find(' ');
    require(null_byte != Bytes::npos && space < null_byte, "Invalid object header");
    if (type != nullptr) {
        *type = raw.substr(0, space);
    }
    return raw.substr(null_byte + 1);
}

FileIndex Repository::read_index() const {
    FileIndex files;
    std::ifstream input(metadata_ / "index");
    std::string line;

    while (std::getline(input, line)) {
        const auto separator = line.find(' ');
        if (separator == 40 && line.size() > 41) {
            files[line.substr(41)] = line.substr(0, 40);
        }
    }
    return files;
}

void Repository::write_index(const FileIndex& entries) const {
    const auto temporary = metadata_ / "index.tmp";
    std::ofstream output(temporary);
    require(static_cast<bool>(output), "Could not write index");

    for (const auto& [path, id] : entries) {
        output << id << ' ' << path << '\n';
    }
    output.close();
    fs::rename(temporary, metadata_ / "index");
}

std::string Repository::head_id() const {
    std::ifstream input(metadata_ / "HEAD");
    std::string value;
    std::getline(input, value);

    if (value.rfind("ref: ", 0) == 0) {
        std::ifstream reference(metadata_ / value.substr(5));
        std::getline(reference, value);
    }
    return value.size() == 40 ? value : std::string{};
}

std::string Repository::commit_tree(const std::string& id) const {
    if (id.empty()) {
        return {};
    }

    std::string type;
    const auto contents = read_object(id, &type);
    require(type == "commit", "HEAD does not point to a commit");
    require(contents.rfind("tree ", 0) == 0, "Malformed commit");
    return contents.substr(5, contents.find('\n') - 5);
}

FileIndex Repository::read_tree(const std::string& id) const {
    FileIndex files;
    if (id.empty()) {
        return files;
    }

    const auto read_subtree = [this, &files](auto&& self,
                                             const std::string& tree_id,
                                             const std::string& prefix) -> void {
        std::string type;
        const auto contents = read_object(tree_id, &type);
        require(type == "tree", "Expected tree object");

        size_t position = 0;
        while (position < contents.size()) {
            const auto space = contents.find(' ', position);
            const auto null_byte = contents.find('\0', space + 1);
            require(space != Bytes::npos && null_byte != Bytes::npos
                    && null_byte + 21 <= contents.size(), "Corrupt tree object");

            const auto mode = contents.substr(position, space - position);
            const auto name = contents.substr(space + 1, null_byte - space - 1);
            std::ostringstream hash;
            hash << std::hex << std::setfill('0');
            for (size_t i = 0; i < 20; ++i) {
                hash << std::setw(2)
                     << static_cast<unsigned>(
                            static_cast<unsigned char>(contents[null_byte + 1 + i]));
            }

            const auto path = prefix.empty() ? name : prefix + "/" + name;
            if (mode == "40000" || mode == "040000") {
                self(self, hash.str(), path);
            } else {
                files[path] = hash.str();
            }
            position = null_byte + 21;
        }
    };

    read_subtree(read_subtree, id, "");
    return files;
}

FileIndex Repository::head_files() const {
    const auto id = head_id();
    return id.empty() ? FileIndex{} : read_tree(commit_tree(id));
}

std::string Repository::write_tree(const FileIndex& files) const {
    struct TreeNode {
        std::map<std::string, TreeNode> directories;
        std::map<std::string, std::string> files;
    };
    TreeNode root;

    for (const auto& [path, id] : files) {
        const fs::path relative(path);
        TreeNode* node = &root;
        for (auto part = relative.begin(); part != relative.end(); ++part) {
            const auto name = part->string();
            auto next = part;
            ++next;
            if (next == relative.end()) {
                node->files[name] = id;
            } else {
                node = &node->directories[name];
            }
        }
    }

    const auto write_node = [this](auto&& self, const TreeNode& node) -> std::string {
        struct Entry {
            std::string name;
            std::string mode;
            std::string id;
            bool directory;
        };
        std::vector<Entry> entries;

        for (const auto& [name, id] : node.files) {
            entries.push_back({name, "100644", id, false});
        }
        for (const auto& [name, child] : node.directories) {
            entries.push_back({name, "40000", self(self, child), true});
        }
        std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
            return left.name + (left.directory ? "/" : "")
                 < right.name + (right.directory ? "/" : "");
        });

        Bytes contents;
        for (const auto& entry : entries) {
            contents += entry.mode + " " + entry.name + '\0';
            for (size_t i = 0; i < entry.id.size(); i += 2) {
                contents.push_back(static_cast<char>(
                    std::stoi(entry.id.substr(i, 2), nullptr, 16)));
            }
        }
        return write_object("tree", contents);
    };

    return write_node(write_node, root);
}

fs::path Repository::worktree_path(const std::string& relative) const {
    const fs::path path(relative);
    require(!path.is_absolute(), "Absolute path rejected");
    for (const auto& part : path) {
        require(part != "..", "Path escapes worktree");
    }
    return (worktree_ / path).lexically_normal();
}

std::vector<std::string> Repository::paths_from_args(int argc, char** argv, int first) const {
    std::vector<std::string> paths;
    for (int i = first; i < argc; ++i) {
        const auto absolute = fs::absolute(argv[i]).lexically_normal();
        const auto relative = absolute.lexically_relative(worktree_);
        require(!relative.empty() && *relative.begin() != "..", "Path outside worktree");
        paths.push_back(relative.generic_string());
    }
    return paths;
}

} // namespace vcs
