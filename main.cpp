#include <iostream>
#include "vcs_cmd.cpp"
using namespace std;



int main(int argc, char *argv[]){
	if(argc != 1){
		vcs_cmd vcm_obj;
		string arg = string(argv[1]);

		if(arg == "init"){
			cout << "inti cmd" << endl;
			vcm_obj.init();
		}
		else if(arg == "add"){
			cout << "add cmd" << endl;
		}
		else if(arg == "commit"){
			cout << "commit cmd" << endl;
		}
		else{
			cout << "Invalid Argument" << endl;
		}
	}
	else{
		cout << "Command Usage: ./a.out <argument> <option>" << endl;
		cout << endl;
		cout << "<argument> :" << endl;
		cout << "	init - intializes the folder" << endl;
		cout << "	add - Tell which files to track" << endl;
		cout << "	commit - Creates a SnapShot of the current folder" << endl;

		cout << endl;
		cout << "<option> :" << endl;
		cout << "	./a.out <argument> --help -> For details on argument usage" << endl;
	}
	return 0;
}
