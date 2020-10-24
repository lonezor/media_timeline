#if 0
/*-------------------------------------------------------------------------------------------------------------------*/

WorkerThread::WorkerThread(SharedThreadData* sharedThreadData, int threadIndex, bool isMaster) {
	memset(&this->threadId, 0, sizeof(this->threadId));
	this->sharedThreadData = sharedThreadData;
	this->isRunning        = FALSE;
	this->threadIndex      = threadIndex;
	this->isMaster         = isMaster;
}

/*-------------------------------------------------------------------------------------------------------------------*/

WorkerThread::~WorkerThread() {

}

/*-------------------------------------------------------------------------------------------------------------------*/

static void* workerThreadMain(void* data) {
	context_t            ctx;
	int                  inotifyFd;
	char                 inotifyBuffer[EVENT_BUF_LEN];
	list<string>         dirPaths;
	list<int>            wdList;
	int                  listeningSock;
	list<int>            clientSockList;
	startupState_t     startupState = STARTUP_STATE_INITIAL;
	//int                  res;

	ctx.workerThread     = this;
	ctx.sharedThreadData = sharedThreadData;
	ctx.options          = sharedThreadData->options;
	ctx.srcFileSystem    = sharedThreadData->srcFileSystem;
	ctx.dstFileSystem    = sharedThreadData->dstFileSystem;

	inotifyFd     = setupInotify();

	/* Initial file system */
	ctx.srcFileSystem->baseDir = ctx.options->inputDir;
	ctx.srcFileSystem->registerFileEventCallback(&ctx, fsFileEventCb); // before scan & watchers
	ctx.srcFileSystem->scan(new Directory(ctx.options->inputDir));
	ctx.srcFileSystem->addDirWatchers(inotifyFd);
	
	sharedThreadData->masterReady = TRUE;

	while(!sharedThreadData->exit) {
		fd_set fdSet;
		struct timeval tv;
		int result;
		int length;

		FD_ZERO(&fdSet);
		FD_SET(inotifyFd, &fdSet);
		FD_SET(listeningSock, &fdSet);

		for(list<int>::iterator it = clientSockList.begin(); it != clientSockList.end(); ++it) {
			int clientSock = *it;
			FD_SET(clientSock, &fdSet);
		}

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		result = select(FD_SETSIZE, &fdSet, NULL, NULL, &tv);

		if (FD_ISSET(inotifyFd, &fdSet)) {

			length = read( inotifyFd, inotifyBuffer, EVENT_BUF_LEN );

			if (result > 0) {
				struct inotify_event* event;
				char* p;

				for(p=inotifyBuffer; p < inotifyBuffer + length;) {
					event = (struct inotify_event*)p;
					p+= sizeof(*event) + event->len;
					ctx.srcFileSystem->fsEventDataAvailable(event);
				}
			}

		} 
	} /* end of master thread server loop */
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool WorkerThread::start() {
	if (!this->isRunning) {
		int status;

		status = pthread_create(&this->threadId, NULL, workerThreadMain, (void*)this);
		if (status != 0) {
			printf("error starting worker thread!\n");
			goto exit;
		}

		this->isRunning = TRUE;
	}

	exit:
	return this->isRunning ;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void fsFileEventCb(void* userData, FileSystem::FileSystemEvent event, uint32_t timestamp, File* file, Directory* affectedDirectory) {
	// disable until made thread safe with new task indication and support video as well.

	context_t* ctx = (context_t*)userData;
	WorkerThread* workerThread = (WorkerThread*)ctx->workerThread;
	printf("%s %s %u %s\n", __FUNCTION__, FileSystem::convertFileSystemEventToString(event).c_str(),
			timestamp, file->getPath().c_str());
//
	//pthread_mutex_lock(&ctx->sharedThreadData->lock);
	//workerThread->addReadImageMetaDataTaskToQueue(ctx, file); // TODO, need handling.... add other tasks below in cb when this is done
	//workerThread->addGenerateSrcFileHashTaskToQueue(ctx, file);
	//workerThread->addGenerateImageTaskToQueue(ctx, file);
	//workerThread->nextRoundRobinIndex(ctx, FALSE);

//	pthread_mutex_unlock(&ctx->sharedThreadData->lock);
}

/*-------------------------------------------------------------------------------------------------------------------*/

void fsDirectoryEventCb(void* userData, FileSystem::FileSystemEvent event, uint32_t timestamp, Directory* directory) {
	context_t* ctx = (context_t*)userData;
	WorkerThread* workerThread = (WorkerThread*)ctx->workerThread;
	string tmpPath;
	string relativePath;

	if (!directory) {
		printf("error: %s called with NULL argument!\n", __FUNCTION__);
		return;
	}

	tmpPath = string(directory->path.c_str()); /* deep copy */
	relativePath = "/" + tmpPath.erase(0, ctx->options->inputDir.size()+1);

	switch (event) {
	case FileSystem::FILE_SYSTEM_EVENT_CREATE:
		workerThread->nextRoundRobinIndex(ctx, FALSE);
		ctx->srcFileSystem->dirList.push_back(directory);
		ctx->srcFileSystem->dirPathMap[relativePath] = directory;
		break;
	case FileSystem::FILE_SYSTEM_EVENT_MODIFY:
		break;
	case FileSystem::FILE_SYSTEM_EVENT_DELETE:
		if (ctx->srcFileSystem->dirPathMap.count(relativePath)) {
			ctx->srcFileSystem->dirPathMap.erase(relativePath);
			// TODO: determine if directory can be safely removed. Also, try to get rid of dirList
			// TODO: need recursive removal...
		}
		break;
	case FileSystem::FILE_SYSTEM_EVENT_UNKNOWN:
	default:
		return;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void WorkerThread::addGenerateSrcFileHashTaskToQueue(context_t* ctx, File* file) {
	GenerateSrcFileHashTask* generateSrcFileHashTask;
	string                   dirPath        = file->dirPath;
	string                   removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);
	string                   targetDirName  = ctx->options->outputDir + removedBaseDir;
	u_int32_t roundRobinIndex = ctx->sharedThreadData->roundRobinIndex;

	if (file->type == File::FILE_TYPE_UNKNOWN) {
		return;
	}

	generateSrcFileHashTask = new GenerateSrcFileHashTask(file, targetDirName);

	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	sharedThreadData->taskQueueBg[roundRobinIndex].push((Task*)generateSrcFileHashTask);
	sharedThreadData->counters.nrOfHashTasks++;
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
}

/*-------------------------------------------------------------------------------------------------------------------*/

void WorkerThread::addReadImageMetaDataTaskToQueue(context_t* ctx, File* file) {
	ReadImageMetaDataTask* readImageMetaDataTask;
	int threadIndex = META_DATA_THREAD_INDEX; /* due to thread safety limitation force execution on a specific thread */
                         /* NOTE: tasks dependent on the meta data CANNOT be executed before this task! */

	if (file->type == File::FILE_TYPE_UNKNOWN) {
		return;
	}

	readImageMetaDataTask = new ReadImageMetaDataTask(file);

	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	sharedThreadData->taskQueueBg[threadIndex].push((Task*)readImageMetaDataTask);
	sharedThreadData->counters.nrOfImageMetaDataTasks++;
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
}

void WorkerThread::addReadVideoMetaDataTaskToQueue(context_t* ctx, File* file) {
	ReadVideoMetaDataTask* readVideoMetaDataTask;
	u_int32_t roundRobinIndex = ctx->sharedThreadData->roundRobinIndex;

	if (file->type == File::FILE_TYPE_UNKNOWN) {
		return;
	}

	readVideoMetaDataTask = new ReadVideoMetaDataTask(file);

	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	sharedThreadData->taskQueueBg[roundRobinIndex].push((Task*)readVideoMetaDataTask);
	sharedThreadData->counters.nrOfVideoMetaDataTasks++;
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
}

bool WorkerThread::metaDataThreadHasEmptyQueue() {
	int threadIndex = META_DATA_THREAD_INDEX; /* due to thread safety limitation force execution on a specific thread */
	/* NOTE: tasks dependent on the meta data CANNOT be executed before this task! */
	size_t size;
	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	size = sharedThreadData->taskQueueBg[threadIndex].size();
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);

	return (size == 0);

}

bool WorkerThread::allWorkerThreadsIdling() {
	bool allIdling = TRUE;
	int i;

	for(i=1; i<MAX_NR_OF_THREADS; i++) {
		if (sharedThreadData->taskIsExecuting[i]) {
			allIdling = FALSE;
		}
	}

	return allIdling;
}

bool WorkerThread::allWorkerThreadsHaveEmptyQueues() {
	size_t size;
	bool allEmpty = TRUE;
	int i;

	for(i=1; i<MAX_NR_OF_THREADS; i++) {
		pthread_mutex_lock(&sharedThreadData->taskQueueLock);
		size = sharedThreadData->taskQueueBg[i].size();
		if (size != 0) {
			allEmpty = FALSE;
		}
		pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
	}

	return allEmpty;
}

bool WorkerThread::generalPurposeThreadsHaveAllEmptyQueues() {
	size_t size;
	bool allEmpty = TRUE;
	int i;

	for(i=FIRST_GENERAL_PURPOSE_THREAD_INDEX; i<MAX_NR_OF_THREADS; i++) {
	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	size = sharedThreadData->taskQueueBg[i].size();
	if (size != 0) {
		allEmpty = FALSE;
	}
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
	}

	return allEmpty;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void WorkerThread::addGenerateImageTaskToQueue(context_t* ctx, File* file) {
	GenerateImageTask* generateImageTask;
	string                 dirPath        = file->dirPath;
	string                 removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);
	string                 targetDirName;
	u_int32_t roundRobinIndex = ctx->sharedThreadData->roundRobinIndex;

	if (file->type == File::FILE_TYPE_UNKNOWN) {
		return;
	}

	//if (file->typeIsImage()) {
	targetDirName  = ctx->options->outputDir + removedBaseDir;
	//} else {
	//	targetDirName  = ctx->options->outputDir + "/videos/" + removedBaseDir;
	//}

	printf("add ImageTask for %s to queue on index %u\n", file->getPath().c_str(), roundRobinIndex);

	generateImageTask = new GenerateImageTask(file, targetDirName,
			GenerateImageTask::IMAGE_SIZE_ALL,
			GenerateImageTask::THUMBNAIL_QUALITY_ALL);
	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	sharedThreadData->taskQueueBg[roundRobinIndex].push((Task*)generateImageTask);
	sharedThreadData->counters.nrOfImageTasks++;
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
}

/*-------------------------------------------------------------------------------------------------------------------*/

void WorkerThread::addGenerateVideoTaskToQueue(context_t* ctx, File* file) {
	GenerateVideoTask* generateVideoTask;
	string                 dirPath        = file->dirPath;
	string                 removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);
	string                 targetDirName;
	u_int32_t roundRobinIndex = ctx->sharedThreadData->roundRobinIndex;

	if (file->type == File::FILE_TYPE_UNKNOWN) {
		return;
	}

	if (file->typeIsImage()) {
		targetDirName  = ctx->options->outputDir + removedBaseDir;
	} else {
		targetDirName  = ctx->options->outputDir + removedBaseDir;
	}

	generateVideoTask = new GenerateVideoTask(file, targetDirName);
	pthread_mutex_lock(&sharedThreadData->taskQueueLock);
	sharedThreadData->taskQueueBg[roundRobinIndex].push((Task*)generateVideoTask);
	sharedThreadData->counters.nrOfVideoTasks++;
	pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
}

/*-------------------------------------------------------------------------------------------------------------------*/

vector<string> WorkerThread::split(string str, string separator) {
	string tmpStr = string(str.c_str()); /* force deep string copy, the string tokenizer alters the string */
	vector<string> res;
	char* s;
	char* token = strtok_r((char*)tmpStr.c_str(), (const char*)separator.c_str(), &s);

	while(token != NULL) {
		res.push_back(string(token));
		token = strtok_r(NULL, separator.c_str(), &s);
	}

	return res;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void WorkerThread::handlePageRequest_header(context_t* ctx, pageRequest_t* pReq, Html* h) {
	h->beginHeader();
	h->beginStyle();

	h->beginCssDeclaration("div.menuBar");
	h->addCssEntry("background-image", "url(static_images/menuBarBg.jpg)");
	h->addCssEntry("background-repeat", "no-repeat");
	h->addCssEntry("height", "192px");
	h->addCssEntry("border", "1px solid #414141");
	h->endCssDeclaration();

	h->beginCssDeclaration("h3");
	h->addCssEntry("font-family", "arial, verdana, tahoma");
	h->addCssEntry("font-size", "14px");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li");
	h->addCssEntry("list-style-type", "circle");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a");
	h->addCssEntry("border", "1px solid #ccc");
	h->addCssEntry("padding", "5px 10px");
	h->addCssEntry("margin", "0 0 5px 0");
	h->addCssEntry("text-decoration", "none");
	h->addCssEntry("color", "#666");
	h->addCssEntry("font-family", "arial, verdana, tahoma");
	h->addCssEntry("font-size", "12px");
	h->addCssEntry("display", "inline-block");
	h->addCssEntry("border-radius", "5px");
	h->addCssEntry("-webkit-border-radius", "5px");
	h->addCssEntry("-moz-border-radius", "5px");
	h->addCssEntry("transition", "all 0.5s");
	h->addCssEntry("-webkit-transition", "all 0.5s");
	h->addCssEntry("-moz-transition", "all 0.5s");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a:link");
	h->addCssEntry("background", "#fffdc3; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a:visited");
	h->addCssEntry("background", "#fffdc3; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a:hover");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a.selectedDir:link");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".tree li a.selectedDir:visited");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a");
	h->addCssEntry("border", "1px solid #ccc");
	h->addCssEntry("padding", "5px 10px");
	h->addCssEntry("margin", "0 0 5px 0");
	h->addCssEntry("text-decoration", "none");
	h->addCssEntry("color", "#666");
	h->addCssEntry("font-family", "arial, verdana, tahoma");
	h->addCssEntry("font-size", "12px");
	h->addCssEntry("display", "inline-block");
	h->addCssEntry("border-radius", "5px");
	h->addCssEntry("-webkit-border-radius", "5px");
	h->addCssEntry("-moz-border-radius", "5px");
	h->addCssEntry("transition", "all 0.5s");
	h->addCssEntry("-webkit-transition", "all 0.5s");
	h->addCssEntry("-moz-transition", "all 0.5s");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a:link");
	h->addCssEntry("background", "#fffdc3; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a:visited");
	h->addCssEntry("background", "#fffdc3; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a:hover");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a.selected:link");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration(".menuBar tr td a.selected:visited");
	h->addCssEntry("background", "#c8e4f8; color: #000; border: 1px solid #94a0b4");
	h->endCssDeclaration();

	h->beginCssDeclaration("h1");
	h->addCssEntry("position", "relative");
	h->addCssEntry("font-size", "70px");
	h->addCssEntry("margin-top", "0");
	h->addCssEntry("font-family", "verdana, arial");
	h->addCssEntry("letter-spacing", "-3px");
	h->addCssEntry("-webkit-text-stroke", "1px white");
	h->endCssDeclaration();

	h->beginCssDeclaration("h1 a");
	h->addCssEntry("color", "#000000");
	h->addCssEntry("text-decoration", "none");
	h->addCssEntry("position", "absolute");
	h->endCssDeclaration();

	h->beginCssDeclaration("div.mainTitle");
	h->addCssEntry("position", "absolute");
	h->addCssEntry("top", "60px");
	h->addCssEntry("left", "50px");
	h->endCssDeclaration();

	h->endStyle();
	h->endHeader();
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t WorkerThread::subStringCount(string str, string subStr) {
	uint32_t          count = 0;
	string::size_type pos   = 0;

	while ((pos = str.find(subStr,pos)) != string::npos) {
		pos += subStr.length();
		count++;
	}

	return count;
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t WorkerThread::stringToUint32(string str) {
	string tmpString = string(str.c_str());

	return (uint32_t) atoi(tmpString.c_str());
}

/*-------------------------------------------------------------------------------------------------------------------*/

string WorkerThread::uint32ToString(uint32_t value) {
	char str[10];
	snprintf(str,sizeof(str),"%u", value);

	return string(str);
}

/*-------------------------------------------------------------------------------------------------------------------*/

int WorkerThread::setupInotify() {
	int inotifyFd;

	inotifyFd = inotify_init1(IN_NONBLOCK);
	if (inotifyFd < 0) {
		printf("inotify_init failed!\n");
	}

	return inotifyFd;
}

enum startupState_t {
  STARTUP_STATE_INITIAL,
  STARTUP_STATE_HASH_TASK_QUEUED,
  STARTUP_STATE_HASH_TASK_COMPLETED,
  STARTUP_STATE_VIDEO_METADATA_TASKS_QUEUED,
  STARTUP_STATE_VIDEO_METADATA_TASKS_COMPLETED,
  STARTUP_STATE_IMAGE_METADATA_TASKS_QUEUED,
  STARTUP_STATE_IMAGE_METADATA_TASKS_COMPLETED,
  STARTUP_STATE_IMAGE_TASKS_QUEUED,
  STARTUP_STATE_IMAGE_TASKS_COMPLETED,
  STARTUP_STATE_VIDEO_TASKS_QUEUED,
  STARTUP_STATE_VIDEO_TASKS_COMPLETED,
  STARTUP_STATE_FINISHED,
};

string WorkerThread::getHashFilePath(context_t* ctx, File* file) {
	string targetDirName;
	string dirPath        = file->dirPath;
	string removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);

	//if (file->typeIsImage()) {
		targetDirName  = ctx->options->outputDir + removedBaseDir;
	//} else {
	//	targetDirName  = ctx->options->outputDir + "/videos/" + removedBaseDir;
	//}

	return targetDirName + "/" + file->getNameWithoutFileType() + ".info";
}

bool WorkerThread::hashFileFoundInFs(context_t* ctx, File* file) {
    struct stat sb;
	bool found;
	int result;

    result = stat(getHashFilePath(ctx, file).c_str(), &sb);
    found = (result == 0);

    if (!found) {
    	printf("%s: hash not found in fs\n", getHashFilePath(ctx, file).c_str());
    }

   // printf("[%s] found %d\n", __FUNCTION__, found);

    return found;
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t WorkerThread::getNrBitsInBf(uint32_t allBitsSet) {
	uint32_t nrOfBits = 0;

	while(allBitsSet > 0) {
		allBitsSet >>= 1;
		nrOfBits++;
	}

	return nrOfBits;
}

bool WorkerThread::allImagesFoundInFs(context_t* ctx, File* file) {
	uint32_t i;
	uint32_t j;
	uint32_t size;
	uint32_t quality;
	struct stat sb;
	bool     found = TRUE;
	int      result;
	string targetDirName;
	string dirPath        = file->dirPath;
	string removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);

	targetDirName  = ctx->options->outputDir + removedBaseDir;

	for(i=0 , size=0x01; i < getNrBitsInBf(GenerateImageTask::IMAGE_SIZE_ALL); i++ , size <<= 1) {
		for(j=0, quality=0x01; j < getNrBitsInBf(GenerateImageTask::THUMBNAIL_QUALITY_ALL); j++, quality <<= 1) {

			if (size > GenerateImageTask::IMAGE_SIZE_THUMBNAIL_ALL && quality == GenerateImageTask::THUMBNAIL_QUALITY_LOW) {
				continue;
			}

			if (file->typeIsVideo() && size > GenerateImageTask::IMAGE_SIZE_THUMBNAIL_ALL) {
				continue;
			}

			string path = targetDirName + "/";
			path += GenerateImageTask::getImageFileName(file, (GenerateImageTask::ImageSize)size, (GenerateImageTask::ThumbNailQuality)quality);

			result = stat(path.c_str(), &sb);
			if (result != 0) {
				found = FALSE;
			}

			if (file->typeIsVideo()) {
				printf("[%s] found %s %d\n", __FUNCTION__, path.c_str(), found);
			}
		}
	}

    return found;
}

bool WorkerThread::fileFoundinFs(string path) {
	struct stat sb;
	int    result;
	bool   found = TRUE;

	result = stat(path.c_str(), &sb);
	if (result != 0) {
		found = FALSE;
	}

	return found;
}

/* TODO: so far, all or nothing approach. Add bit field later to specify exactly what is missing */
bool WorkerThread::allVideosFoundinFs(context_t* ctx, File* file) {
	struct stat sb;
	int    result;
	bool   found = TRUE;
	string targetDirName;
	string path;
	string dirPath        = file->dirPath;
	string removedBaseDir = dirPath.erase(0, ctx->options->inputDir.size()+1);

	if (!file->typeIsVideo()) return TRUE;

	targetDirName  = ctx->options->outputDir + "/videos/" + removedBaseDir;

	if (file->videoMetaData.width == 0 || file->videoMetaData.height == 0) {
		path = targetDirName + "/" + file->getNameWithoutFileType();
		printf("no metadata for %s\n", path.c_str());
		return TRUE;
	}

	/* Landscape orientation */
	if (file->videoMetaData.width > file->videoMetaData.height) {
		if (file->videoMetaData.width > 3000) {
			path = targetDirName + "/" + file->getNameWithoutFileType() + "_4k.webm";
			printf("landscape oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
			found &= fileFoundinFs(path);
		}

		if (file->videoMetaData.width > 1800) {
			path = targetDirName + "/" + file->getNameWithoutFileType() + "_2k.webm";
			printf("landscape oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
			found &= fileFoundinFs(path);
		}

		if (file->videoMetaData.width > 1000) {
			path = targetDirName + "/" + file->getNameWithoutFileType() + "_1k.webm";
			printf("landscape oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
			found &= fileFoundinFs(path);
		}

		/* Portrait orientation */
	} else {
		if (file->videoMetaData.height > 3000) {
				path = targetDirName + "/" + file->getNameWithoutFileType() + "_4k.webm";
				printf("portrait oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
				found &= fileFoundinFs(path);
			}

			if (file->videoMetaData.height > 1800) {
				path = targetDirName + "/" + file->getNameWithoutFileType() + "_2k.webm";
				printf("portrait oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
				found &= fileFoundinFs(path);
			}

			if (file->videoMetaData.height > 1000) {
				path = targetDirName + "/" + file->getNameWithoutFileType() + "_1k.webm";
				printf("portrait oriented %s: %s\n", path.c_str(), fileFoundinFs(path) ? "found" : "not found");
				found &= fileFoundinFs(path);
			}
	}

    return found;
}

/*-------------------------------------------------------------------------------------------------------------------*/

string WorkerThread::getHashStringFromFilePath(context_t* ctx, string path) {
#define PREFIX_LEN 6
	FILE*     hashFile;
	uint32_t  i;
	uint32_t  fileSize;
	uint32_t  expectedFileSize = PREFIX_LEN + (FILE_HASH_SIZE*2) + 1;
	uint8_t*  persistentHash;
	uint8_t*  pos;
	size_t    result;
	string    hashString = "";

	hashFile = fopen(path.c_str(), "r");
	if (hashFile == NULL) {
		printf("unable to read %s\n", path.c_str());
		return "";
	}

	fseek(hashFile, 0, SEEK_END);
	fileSize = ftell(hashFile);
	rewind(hashFile);

	if (fileSize != expectedFileSize) {
		printf("incorrect length of %s (real size %u but expected %u)\n", path.c_str(), fileSize, expectedFileSize);
		return "";
	}

	persistentHash = (uint8_t*) malloc(fileSize);
	assert(persistentHash);

	result = fread(persistentHash, 1, fileSize, hashFile);
	if (result != fileSize) {
		printf("could not read all bytes in %s\n", path.c_str());
		free(persistentHash);
		fclose(hashFile);
		return "";
	}

	pos = persistentHash + PREFIX_LEN;
	for(i=0; i<FILE_HASH_SIZE; i++) {
		char str[3];
		uint8_t byte = 0;

		str[0] = *(pos);
		str[1] = *(pos+1);
		str[2] = 0;

		hashString += string(str);
		pos += 2;
	}

	free(persistentHash);
	fclose(hashFile);



	return hashString;
}

bool WorkerThread::hashFileMatchCurrentHash(context_t* ctx, File* file) {
    #define PREFIX_LEN 6
	FILE*     hashFile;
    uint32_t  i;
	uint32_t  fileSize;
    uint32_t  expectedFileSize = PREFIX_LEN + (sizeof(file->hash)*2) + 1;
	string    path = getHashFilePath(ctx, file);
	uint8_t*  persistentHash;
	uint8_t*  pos;
	size_t    result;
	bool      identical = FALSE;

	hashFile = fopen(path.c_str(), "r");
	if (hashFile == NULL) {
		printf("unable to read %s\n", path.c_str());
		return identical;
	}

	fseek(hashFile, 0, SEEK_END);
	fileSize = ftell(hashFile);
	rewind(hashFile);

	if (fileSize != expectedFileSize) {
		printf("incorrect length of %s (real size %u but expected %u)\n", path.c_str(), fileSize, expectedFileSize);
		return identical;
	}

	persistentHash = (uint8_t*) malloc(fileSize);
    assert(persistentHash);

    result = fread(persistentHash, 1, fileSize, hashFile);
    if (result != fileSize) {
    	printf("could not read all bytes in %s\n", path.c_str());
    	free(persistentHash);
    	fclose(hashFile);
    	return identical;
    }

    pos = persistentHash + PREFIX_LEN;
    identical = TRUE;
    for(i=0; i<sizeof(file->hash); i++) {
    	char str[3];
    	uint8_t byte = 0;

    	str[0] = *(pos);
    	str[1] = *(pos+1);
    	str[2] = 0;

    	byte = strtol(str, NULL, 16);

    	//printf("%s index %u byte %02x hash %02x --> %d\n", path.c_str(), i, byte, file->hash[i], (byte == file->hash[i]));
    	identical &= (byte == file->hash[i]);

    	pos += 2;
    }

   	free(persistentHash);
   	fclose(hashFile);

   	//printf("[%s] identical %d\n", __FUNCTION__, identical);

	return identical;
}


void WorkerThread::relocateDstFiles(context_t* ctx, File* oldDstHashFile, File* srcFile) {
	string dstRemovedBaseDir;
	string srcDirPath        = srcFile->dirPath;
	string srcRemovedBaseDir = srcDirPath.erase(0, ctx->options->inputDir.size() +1);
	string newPath = "";
	if (srcRemovedBaseDir != "") {
		srcRemovedBaseDir = "/" + srcRemovedBaseDir;
	}
	string imgDir = "/images";
	dstRemovedBaseDir = oldDstHashFile->dirPath;
	dstRemovedBaseDir = dstRemovedBaseDir.erase(0, ctx->options->outputDir.size() + imgDir.size() +1);
	if (dstRemovedBaseDir != "") {
		dstRemovedBaseDir = "/" + dstRemovedBaseDir;
	}

	string imageStemPath = oldDstHashFile->dirPath + "/" + oldDstHashFile->getNameWithoutFileType();
	string videoStemPath = ctx->options->outputDir + "/videos" + dstRemovedBaseDir + "/" + oldDstHashFile->getNameWithoutFileType();

	for(list<Directory*>::iterator dirIt = ctx->dstFileSystem->dirList.begin(); dirIt != ctx->dstFileSystem->dirList.end(); ++dirIt) {
		Directory* dir = *dirIt;

		for(list<File*>::iterator fileIt = dir->fileList.begin(); fileIt != dir->fileList.end(); ++fileIt) {
			File* file = *fileIt;

			//printf("mathcing %s against video stem path %s\n", file->getPath().c_str(), videoStemPath.c_str());

			if (Common::subStringCount(file->getPath(), imageStemPath) != 0) {

				//printf("file to be relocated: %s\n", file->getPath().c_str());

				if (Common::subStringCount(file->getPath(), ".info") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + ".info";
				}

				if (Common::subStringCount(file->getPath(), "0160_q1") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0160_q1.png";
				}

				if (Common::subStringCount(file->getPath(), "0160_q2") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0160_q2.jpg";
				}

				if (Common::subStringCount(file->getPath(), "0320_q1") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0320_q1.png";
				}

				if (Common::subStringCount(file->getPath(), "0320_q2") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0320_q2.jpg";
				}

				if (Common::subStringCount(file->getPath(), "0640_q1") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0640_q1.png";
				}

				if (Common::subStringCount(file->getPath(), "0640_q2") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_0640_q2.jpg";
				}


				if (Common::subStringCount(file->getPath(), "1024") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_1024.jpg";
				}

				if (Common::subStringCount(file->getPath(), "2048") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_2048.jpg";
				}

				if (Common::subStringCount(file->getPath(), "4096") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_4096.jpg";
				}

				if (Common::subStringCount(file->getPath(), "nonLetterBoxed") != 0) {
					newPath = ctx->options->outputDir + "/images" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_nonLetterBoxed.jpg";
				}

				printf("Moving %s to %s\n", file->getPath().c_str(), newPath.c_str());
				FileSystem::moveFile(file->getPath(), newPath);
				// TODO: if old directories becomes empty, remove them

				pthread_mutex_lock(&sharedThreadData->taskQueueLock);
				sharedThreadData->counters.nrOfRelocatedDstFiles_dstImagesMoved++;

				if (Common::subStringCount(file->getPath(), ".info") != 0) {
					sharedThreadData->counters.nrOfRelocatedDstFiles_srcImagesMoved++;
				}
				pthread_mutex_unlock(&sharedThreadData->taskQueueLock);

			}
			else if (Common::subStringCount(file->getPath(), videoStemPath) != 0) {
				//printf("%s matching video stem path %s\n", file->getPath().c_str(), videoStemPath.c_str());

				if (Common::subStringCount(file->getPath(), "_1k") != 0) {
					newPath = ctx->options->outputDir + "/videos" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_1k.webm";
				}

				if (Common::subStringCount(file->getPath(), "_2k") != 0) {
					newPath = ctx->options->outputDir + "/videos" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_2k.webm";
				}

				if (Common::subStringCount(file->getPath(), "_4k") != 0) {
					newPath = ctx->options->outputDir + "/videos" + srcRemovedBaseDir + "/" + srcFile->getNameWithoutFileType() + "_4k.webm";
				}

				printf("Moving %s to %s\n", file->getPath().c_str(), newPath.c_str());
				FileSystem::moveFile(file->getPath(), newPath);
				// TODO: if old directories becomes empty, remove them

				pthread_mutex_lock(&sharedThreadData->taskQueueLock);
				sharedThreadData->counters.nrOfRelocatedDstFiles_dstVideosMoved++;

				if (Common::subStringCount(file->getPath(), ".info") != 0) {
					sharedThreadData->counters.nrOfRelocatedDstFiles_srcVideosMoved++;
				}
				pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
			}
		}/* end of file loop */
	} /* end of dir loop */
}

void WorkerThread::removeDstFiles(context_t* ctx, File* oldDstHashFile) {
	string stemPath = oldDstHashFile->dirPath + "/" + oldDstHashFile->getNameWithoutFileType();

	for(list<Directory*>::iterator dirIt = ctx->dstFileSystem->dirList.begin(); dirIt != ctx->dstFileSystem->dirList.end(); ++dirIt) {
		Directory* dir = *dirIt;

		for(list<File*>::iterator fileIt = dir->fileList.begin(); fileIt != dir->fileList.end(); ++fileIt) {
			File* file = *fileIt;

			if (Common::subStringCount(file->getPath(), stemPath) != 0) {
				printf("Removing %s\n", file->getPath().c_str());
				FileSystem::removeFile(file->getPath());

				pthread_mutex_lock(&sharedThreadData->taskQueueLock);
				sharedThreadData->counters.nrOfRemovedDstFiles++;
				pthread_mutex_unlock(&sharedThreadData->taskQueueLock);
			}
		}
	}

}

int32_t WorkerThread::getTid() {
	return syscall(SYS_gettid);
}

/*-------------------------------------------------------------------------------------------------------------------*/

#endif