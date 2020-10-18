#include "GenerateSrcFileHashTask.h"
#include "SharedThreadData.h"
#include "general.h"

#include <mhash.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <list>
#include <errno.h>
#include "FileSystem.h"

/*-------------------------------------------------------------------------------------------------------------------*/

GenerateSrcFileHashTask::GenerateSrcFileHashTask(File* file) {
	this->file          = file;
}

/*-------------------------------------------------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------------------------------------------------*/

void GenerateSrcFileHashTask::execute() {
	printf("[%s GenerateSrcFileHashTask] %s \n", __FUNCTION__, file->getPath().c_str());
	generateHash();
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateSrcFileHashTask::generateHash() {
	uint8_t*  srcBuffer;
	uint32_t  bytesRemaining;
	MHASH     td;
	int       srcFd;
	int       result;
	string    srcPath = file->getPath();
#define DATA_BUFFER_SIZE (1*1024*1024)

	printf("[%s] %s\n", __FUNCTION__, srcPath.c_str());

	td = mhash_init(MHASH_SHA1);
	if (td == MHASH_FAILED) {
		printf("[%s] failed to initialize hash function for file %s\n", __FUNCTION__, srcPath.c_str());
		return FALSE;
	}

	srcFd = open(srcPath.c_str(), O_RDONLY);
	if (srcFd == -1) {
		printf("[%s] failed to open source file %s\n", __FUNCTION__, srcPath.c_str());
		mhash_deinit(td, file->hash);
		return FALSE;
	}

	bytesRemaining = file->size;

	srcBuffer = (uint8_t*) calloc(1, DATA_BUFFER_SIZE);
	assert(srcBuffer);

	while (bytesRemaining > 0) {
		if (bytesRemaining >= DATA_BUFFER_SIZE) {
			result = read(srcFd, srcBuffer, DATA_BUFFER_SIZE);
		} else {
			result = read(srcFd, srcBuffer, bytesRemaining);
		}

		if (result == -1) {
			printf("[%s] failed to read file %s (%u bytes)\n", __FUNCTION__, srcPath.c_str(), file->size);
			mhash_deinit(td, file->hash);
			free(srcBuffer);
			close(srcFd);
			return FALSE;
		} else if (result == 0) {
			printf("[%s] warning, should not be here\n", __FUNCTION__);
			break;
		}

		mhash(td, srcBuffer, result);

		bytesRemaining -= result;
	}

	mhash_deinit(td, file->hash); /* hash value is now accessible */

	free(srcBuffer);
	close(srcFd);

	printf("%s: %s\n", file->getPath().c_str(), file->getHashString().c_str());

	return TRUE;
}

/*-------------------------------------------------------------------------------------------------------------------*/


