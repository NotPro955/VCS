#include <filesystem>
using namespace std;

class vcs_cmd {
public:
	void init();
	void add();
	void commit();
};

void vcs_cmd::init(){
	filesystem::create_directory(".vcs");
	filesystem::create_directory(".vcs/commits");
	cout << "Created the VCS directory (.vcs)" << endl;
}
