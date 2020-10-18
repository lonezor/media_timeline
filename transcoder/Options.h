#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <string>
#include <stdint.h>

using namespace std;

class Options {
    public:
        string     inputDir;
        string     outputDir;
        int        nrWorkers;

        void print();
};

#endif /* OPTIONS_H_ */

