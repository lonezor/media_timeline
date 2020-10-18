#ifndef READAUDIOEMETADATATASK_H_
#define READAUDIOMETADATATASK_H_


#include "Task.h"
#include "File.h"

#include <string>

using namespace std;

class ReadAudioMetaDataTask : Task {
public:
	ReadAudioMetaDataTask(File* file);
    virtual void execute(void);

private:
    string extractAttributeValue(string line);
    string extractDuration(string line);
    File*  file;
};


#endif /* READAUDIOMETADATATASK_H_ */
