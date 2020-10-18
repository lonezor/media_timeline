#ifndef GENERATE_AUDIO_TASK_H_
#define GENERATE_AUDIO_TASK_H_

#include "Task.h"
#include "File.h"

#include <string>

using namespace std;

class GenerateAudioTask : Task {

public:

	GenerateAudioTask(File* file, string targetDirName);

	virtual void execute(void);

    File*             file;
    string            targetDirName;
};


#endif /* GENERATE_AUDIO_TASK_H_ */
