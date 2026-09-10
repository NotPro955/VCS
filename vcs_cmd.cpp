#include <filesystem>
#include <fstream>
using namespace std;

class vcs_cmd {
public:
	void init();
	void add(string added_file);
	void commit();
};

void vcs_cmd::init(){
	if(filesystem::is_directory(".vcs")){
		cout << "Folder Already Initialized" << endl;
	}
	else{
		filesystem::create_directory(".vcs");
		filesystem::create_directory(".vcs/commits");
		cout << "Created the VCS directory (.vcs)" << endl;
	}
}

void vcs_cmd::add(string added_file){
	ofstream file("./.vcs/file_track.txt");
	if(!file.is_open()){
		cerr << "File error" << endl;
	}
	for(const auto& entry: filesystem::recursive_directory_iterator(added_file)){
		file << entry.path() << "\n";

	}
	file.close();
}
