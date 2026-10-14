#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

class VcsRepo {
public:
	filesystem::path worktree;
	filesystem::path gitdir;

	explicit VcsRepo(const filesystem::path& path)
		: worktree(path), gitdir(worktree / ".vcs") {}
};

void repo_create(const filesystem::path& path) {
	VcsRepo repo(path);

	if (!filesystem::is_directory(repo.worktree)) {
		cerr << repo.worktree << " is not an existing directory!" << endl;
		return;
	}

	if (filesystem::exists(repo.gitdir)) {
		if (!filesystem::is_directory(repo.gitdir)) {
			cerr << repo.gitdir << " exists and is not a directory!" << endl;
			return;
		}
		if (!filesystem::is_empty(repo.gitdir)) {
			cerr << repo.gitdir << " is not empty!" << endl;
			return;
		}
	} else {
		filesystem::create_directory(repo.gitdir);
	}

	filesystem::create_directories(repo.gitdir / "branches");
	filesystem::create_directories(repo.gitdir / "objects");
	filesystem::create_directories(repo.gitdir / "refs" / "tags");
	filesystem::create_directories(repo.gitdir / "refs" / "heads");

	ofstream description(repo.gitdir / "description");
	description << "Unnamed repository; edit this file\n";
	ofstream head(repo.gitdir / "HEAD");
	head << "ref: refs/heads/main\n";
	ofstream config(repo.gitdir / "config");
	config << "[core]\n"
	       << "\trepositoryformatversion = 0\n"
	       << "\tfilemode = false\n"
	       << "\tbare = false\n";

	if (!description || !head || !config) {
		cerr << "Failed to write repository metadata in " << repo.gitdir << endl;
	}
}

void cmd_init(int argc, char* args[]) {
	if (argc > 3) {
		cerr << "Usage: " << args[0] << " init [path]" << endl;
		return;
	}

	filesystem::path path = argc == 3 ? filesystem::path(args[2]) : filesystem::current_path();
	if (path == ".") {
		path = filesystem::current_path();
	}

	try {
		repo_create(path);
	} catch (const filesystem::filesystem_error& error) {
		cerr << "Repository initialization failed: " << error.what() << endl;
	}
}
