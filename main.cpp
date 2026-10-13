#include <iostream>
#include "cmd_args.cpp"
using namespace std;

int main(int argc,char *argv[]){
	string cmd = argv[1];
	if (cmd == "init")
		cmd_init(argv);
	else
		printf("Invalid Command\n");
	return 0;
}
