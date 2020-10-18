#ifndef SHELLCMD_H_
#define SHELLCMD_H_

#include <string>
#include <stdint.h>

using namespace std;

class ShellCmd {
    public:
        static string execute(string cmd);
};


#endif /* SHELLCMD_H_ */
