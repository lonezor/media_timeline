#ifndef GENERATESRCFILEHASHTASK_H_
#define GENERATESRCFILEHASHTASK_H_

#include "Task.h"

#include <string>

using namespace std;

class GenerateSrcFileHashTask : Task {
public:
	GenerateSrcFileHashTask(File* file);

    virtual void execute(void);

private:
    bool generateHash();

    File*  file;
};

#endif /* GENERATESRCFILEHASHTASK_H_ */
