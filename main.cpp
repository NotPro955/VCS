#include <iostream>
#include "cmd_args.cpp"
using namespace std;

int main(int argc,char *argv[]){
	if (argc < 2) {
		printf("Usage: %s <command> [path]\n", argv[0]);
		return 1;
	}

	string cmd = argv[1];

	if (cmd == "init")
		cmd_init(argc, argv);
	else
		printf("Invalid Command\n");
	return 0;
}
