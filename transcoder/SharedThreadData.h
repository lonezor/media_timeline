#ifndef SHAREDTHREADDATA_H_
#define SHAREDTHREADDATA_H_

#include <queue>

#include "pthread.h"
#include "Options.h"
#include "FileSystem.h"
#include "Task.h"

#define MAX_NR_OF_THREADS 16
#define META_DATA_THREAD_INDEX 1
#define FIRST_GENERAL_PURPOSE_THREAD_INDEX 2

using namespace std;

class SharedThreadData {

	struct Counters {
		uint32_t nrOfHashTasks;
		uint32_t nrOfImageMetaDataTasks;
		uint32_t nrOfVideoMetaDataTasks;
		uint32_t nrOfImageTasks;
		uint32_t nrOfVideoTasks;

		uint32_t nrOfRemovedDstFiles;
		uint32_t nrOfRelocatedDstFiles_dstImagesMoved;
		uint32_t nrOfRelocatedDstFiles_srcImagesMoved;
		uint32_t nrOfRelocatedDstFiles_dstVideosMoved;
		uint32_t nrOfRelocatedDstFiles_srcVideosMoved;

		uint32_t nrOfAuthRequests;
		uint32_t nrOfPageRequests;
		uint32_t nrOfDebugRequests;
	};

public:
	SharedThreadData();
	~SharedThreadData();

	/*** Lock shared between master and slave threads ***/
	pthread_mutex_t lock; /* top level lock */
	pthread_mutex_t taskQueueLock;

	/*** Data not needing any lock operations ***/
	Options*        options;
	bool            masterReady;
	bool            exit;
	uint32_t        roundRobinIndex;
	bool            taskIsExecuting[MAX_NR_OF_THREADS];

	/*** Read-writable data that must be protected with lock ***/
	FileSystem*     srcFileSystem;
	FileSystem*     dstFileSystem;
	queue<Task*>    taskQueueBg[MAX_NR_OF_THREADS];    /* Background job queue. May take hours to process */
	uint32_t        hashGenCounter; /* Number of completed file hashes */
	Counters        counters;
};

#endif /* SHAREDTHREADDATA_H_ */
