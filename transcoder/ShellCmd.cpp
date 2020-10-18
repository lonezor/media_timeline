#include "general.h"
#include "ShellCmd.h"
#include "Common.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 100
#define BUF_SIZE 1000

string ShellCmd::execute(string cmd) {
	string output = "";
	FILE* f;
	char buff[BUF_SIZE];

	memset(buff, 0, sizeof(buff));

	cmd += " 2>&1"; /* redirect stderr to stdout */

	f = popen(cmd.c_str(), "r");

	while (fgets(buff, BUF_SIZE, f) != NULL) {
		output += string(buff);
	}

	pclose(f);

	//printf("output:\n%s", output.c_str());

	return output;
}
