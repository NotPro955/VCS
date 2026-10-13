#include <filesystem>
#include <fstream>
#include <chrono>
#include <openssl/sha.h>
using namespace std;

class VcsRepo{
	public:
		filesystem::path worktree,gitdir,conf;
	
		VcsRepo(string path, bool force=false){
			worktree = filesystem::path(path);

			gitdir = worktree;
			gitdir.append("/.vcs");

			if(filesystem::is_directory(gitdir)){
				cout << "Not a VCS Directory" << endl;
			}

			conf = gitdir;
			conf.append("/config");
	
			if(filesystem::exists(conf)){
				ifstream file(conf);
	
				if (!file.is_open()){
					cerr << "Error Config: file opening" << endl;
				}

				string line;
				while(std::getline(file, line)){
					// need to finish the repo format version check
				}

				file.close();
			}
			else{
				cout << "Config file not found" << endl;
			}
		}
};




void repo_create(char *args[]){
	VcsRepo repo(args[2],true);
	
	if (filesystem::exists(repo.worktree)){
		if (!filesystem::is_regular_file(repo.worktree)){
			cout << repo.worktree << " is not a directory!" << endl;
		}
		if (filesystem::exists(repo.worktree) && filesystem::directory_iterator(repo.worktree) != filesystem::directory_iterator{}){
			cout << repo.worktree << " is not empty!" << endl;
		}
	}
	else{
		filesystem::create_directories(repo.worktree);
	}

	filesystem::create_directories(repo.gitdir / "branches");
	filesystem::create_directories(repo.gitdir / "objects");
	filesystem::create_directories(repo.gitdir / "refs" / "tags" );
	filesystem::create_directories(repo.gitdir / "refs" / "heads");

	ofstream descrip(repo.gitdir/"description");
	descrip << "Unnamed repository; edit this file" << endl;
	descrip.close();

	ofstream head(repo.gitdir / "HEAD");
	head << "ref: refs/heads/main\n" << endl;
	head.close();

	ofstream conf(repo.gitdir / "config");
	conf << "[core]\n"
		<< "\trepositoryformatversion = 0\n"
		<< "\tfilemode = false\n"
		<< "\tbare = false\n";
}


void cmd_init(char *args[]){
	repo_create(args);
}
