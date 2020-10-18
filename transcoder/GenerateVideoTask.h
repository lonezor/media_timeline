#ifndef GENERATEVIDEOTASK_H_
#define GENERATEVIDEOTASK_H_

#include "Task.h"
#include "File.h"

#include <string>

using namespace std;

class GenerateVideoTask : Task {
public:
	GenerateVideoTask(File* file, string targetDirName);

    virtual void execute(void);

private:

    string getHashFilePath(void);
    bool writeHashFile(void);

    File*  file;
    string targetDirName;
};


#endif /* GENERATEVIDEOTASK_H_ */
