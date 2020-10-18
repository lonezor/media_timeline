#include "SharedThreadData.h"

#include "general.h"
#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------------------------*/

SharedThreadData::SharedThreadData() {
	pthread_mutex_init(&lock, NULL);
	pthread_mutex_init(&taskQueueLock, NULL);
	this->options         = new Options();
	this->srcFileSystem   = new FileSystem();
	this->dstFileSystem   = new FileSystem();
	this->masterReady     = FALSE;
	this->exit            = FALSE;
	this->hashGenCounter  = 0;
	this->roundRobinIndex = FIRST_GENERAL_PURPOSE_THREAD_INDEX; /* do not start with index 1 to cover case first file is video */

	memset(taskIsExecuting, 0, sizeof(taskIsExecuting));
	memset(&counters, 0, sizeof(counters));
}

/*-------------------------------------------------------------------------------------------------------------------*/

SharedThreadData::~SharedThreadData() {
	delete this->options;
	delete this->srcFileSystem;
}

/*-------------------------------------------------------------------------------------------------------------------*/
