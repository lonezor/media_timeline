#ifndef READVIDEOMETADATATASK_H_
#define READVIDEOMETADATATASK_H_

#include "Task.h"
#include "File.h"

#include <string>

using namespace std;

class ReadVideoMetaDataTask : Task {
public:
	ReadVideoMetaDataTask(File* file);
    virtual void execute(void);

private:
    File*  file;
};


#endif /* READVIDEOMETADATATASK_H_ */
