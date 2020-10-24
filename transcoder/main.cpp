#include "FileSystem.h"
#include "general.h"
#include "GenerateSrcFileHashTask.h"
#include "GenerateImageTask.h"
#include "GenerateAudioTask.h"
#include "GenerateVideoTask.h"
#include "ReadImageMetaDataTask.h"
#include "ReadVideoMetaDataTask.h"
#include "ReadAudioMetaDataTask.h"
#include "SyncWithFsTask.h"
#include "ShellCmd.h"
#include "Common.h"
#include "Html.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>

#include <stdbool.h>
#include <queue>

#define EVENT_SIZE  (sizeof (struct inotify_event))
#define EVENT_BUF_LEN (1024*(EVENT_SIZE+16))
#define MAX_PATH_LEN (2048)

#define DEFAULT_NR_WORKERS (4)

using namespace std;

/*-------------------------------------------------------------------------------------------------------------------*/

static int maxWorkers = DEFAULT_NR_WORKERS;

static char inputDir[MAX_PATH_LEN]  = {0};
static char inputFile[MAX_PATH_LEN] = {0};
static char outputDir[MAX_PATH_LEN] = {0};

static int  forceRotation = 0;
static bool forceVerticalFlip = false;
static bool forceHorizontalFlip = false;
static bool helpScreen = false;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static queue<char*> pathQueue;
static int activeWorkers = 0;

/*-------------------------------------------------------------------------------------------------------------------*/

void printHelp() {
	printf("%s %s (%s)\n", PROGRAM_NAME, PROGRAM_VERSION, PROGRAM_DATE);
	printf("\nUsage: %s [OPTIONS]\n", PROGRAM_NAME);

	printf("File Detection Mode:\n");
	printf(" -i --input-dir=PATH          Directory path containing media files (original files will be moved)\n");
	printf(" -w --workers=INT             Maximum number of concurrent workers\n");
	printf("\n");

	printf("File Processing Mode:\n");
	printf(" -f --input-file=PATH          File path that should be processed\n");
	printf(" -o  --output-dir=PATH         Directory path where output files are written\n");
	printf("     --force-rotation=DEGREES Force rotation of media to specified degree\n");
	printf("     --force-vertical-flip    Force flipping media vertically\n");
	printf("     --force-horizontal-flip  Force flipping media horizontally\n");
	printf("\n");
}

/*----------------------------readImageMetaDataTask---------------------------------------------------------------------------------------*/

void parseArguments(int argc, char* argv[]) {
	snprintf(inputDir, sizeof(inputDir), "%s", "/home/lonezor/Media/media_timeline");
	snprintf(outputDir, sizeof(outputDir), "%s", "/home/output");

	if (argc > 1) {
		snprintf(inputFile, sizeof(inputFile), "%s", argv[1]);
	}

	printf("inputDir: %s\n", inputDir);
	printf("outputDir: %s\n", outputDir);
	printf("inputFile: %s\n", inputFile);
}

/*-------------------------------------------------------------------------------------------------------------------*/

void fsFileEventCb(FileSystem::FileSystemEvent event, uint32_t timestamp, File* file, Directory* affectedDirectory, void* userData) {
	if (file->typeIsImage() || file->typeIsAnimation() || file->typeIsVideo() || file->typeIsAudio()) {
		string path = file->getPath();

		struct tm* timeInfo;
		char timeStr[50];
		const time_t ts = (const time_t)timestamp;

		timeInfo = localtime(&ts);

		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeInfo);


		printf("[%s] %s %s\n",
				timeStr,
				FileSystem::convertFileSystemEventToString(event).c_str(),
				path.c_str());

		if (event == FileSystem::FILE_SYSTEM_EVENT_CREATE) {
			pthread_mutex_lock(&mutex);
			pathQueue.push(strdup(path.c_str()));
			pthread_mutex_unlock(&mutex);
		}
	}
}

void* execThread(void* data)
{
	char* path = (char*)data;
	assert(path);

	printf("Thread started to spawn process for %s\n", path);

	pid_t pid = fork();

	// Error handling
	if (pid < 0)
	{
    	printf("[%s] fork() failed!\n");
    	assert(0);
    }

    // Parent
	else if (pid > 0)
	{
    	int status;
    	waitpid(pid, &status, 0);

    	//unlink

    	pthread_mutex_lock(&mutex);
		activeWorkers--;
		pthread_mutex_unlock(&mutex);
	}

	// Child
	else 
	{
		printf("Child %d created for path %s\n", getpid(), path);
    	char* args[] = {"mtransc", path, NULL};
		execv("/usr/bin/mtransc", args);
    	exit(0);
	}

	free(path);

	return NULL;
}

void* queueThread(void* data)
{
	printf("queueThread enter\n");
	while(true) {
		char* path = NULL;

		pthread_mutex_lock(&mutex);

		if (!pathQueue.empty() && activeWorkers < maxWorkers) {
			path = pathQueue.front();
			pathQueue.pop();

			if (path) {

				pthread_t thread;
				int res = 0;

				res = pthread_create(&thread, NULL, execThread, path);
				if (res != 0) {
					printf("[%s] pthread_create() failed!\n", __FUNCTION__);
					assert(0);
				}
				res = pthread_detach(thread);
				if (res != 0) {
					printf("[%s] pthread_detach() failed!\n", __FUNCTION__);
					assert(0);
				}

				activeWorkers++;

				printf("[activeWorkers %d] pop from queue: %s\n", activeWorkers, path);
			}
		}

		pthread_mutex_unlock(&mutex);

		usleep(5000);
	}

	return NULL;
}



int fileDetectionMode()
{
	pthread_t thread;
	int res = 0;

	res = pthread_create(&thread, NULL, queueThread, NULL);
	if (res != 0) {
		printf("[%s] pthread_create() failed!\n", __FUNCTION__);
		return -1;
	}
	res = pthread_detach(thread);
	if (res != 0) {
		printf("[%s] pthread_detach() failed!\n", __FUNCTION__);
		return -1;
	}

	FileSystem* fs = new FileSystem();
	string baseDir = string(inputDir);
	fs->baseDir = baseDir;
	fs->registerFileEventCallback(fsFileEventCb, NULL); // before scan & watchers

	printf("File Detection Mode\n");

	// Everytime a file has been identified via directory scanning or
	// file creation event the path will be added to a queue. A background
	// thread is responsible for processing 
	//char inotifyBuffer[EVENT_BUF_LEN];
	//int inotifyFd = inotify_init1(IN_NONBLOCK);
	//if (inotifyFd < 0) {
//		printf("inotify_init failed!\n");
//		res = -1;
//		goto exit;
//	}

	// Process files that exist before startup
	fs->scan(new Directory(baseDir));


	while(true) {
		sleep(1);
		int size = 0;
		pthread_mutex_lock(&mutex);
		size = pathQueue.size();
		pthread_mutex_unlock(&mutex);
		if (activeWorkers == 0 && size == 0) {
			break;
		}

	}
	

	// Take care of new files based on Inotify events
	#if 0
	fs->addDirWatchers(inotifyFd);
	while(true) {
		fd_set fdSet;
		struct timeval tv;
		int result;
		int length;

		FD_ZERO(&fdSet);
		FD_SET(inotifyFd, &fdSet);

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
					fs->fsEventDataAvailable(event);
				}
			}

		} 
	} /* end of master thread server loop */
#endif
exit:
	return res;
}

int fileProcessingMode()
{
	FreeImage_Initialise(0);

	printf("File Processing Mode\n");

	int res = 0;

	char* str1 = strdup(inputFile);
	char* dirName = dirname(str1);

	char* str2 = strdup(inputFile);
	char* baseName = basename(str2);

	char* str3 = NULL;
	char jpgPath[512] = {0};

	printf("analysing %s\n", dirName);

	File* file = new File(string(dirName), string(baseName));
	file->analyze();
	file->print();

	if (file->getHashString() == "") {
		GenerateSrcFileHashTask* generateSrcFileHashTask = new GenerateSrcFileHashTask(file);
		generateSrcFileHashTask->execute();
		delete generateSrcFileHashTask;
	}

	if (FileSystem::fileExists(file->getPath() + ".info")) {
		printf("Already processed. Ignoring\n");
		return 0;
	}

	printf("file type %d\n", file->type);

	// HEIC file must be converted to JPG
	if (file->type == File::FILE_TYPE_HEIC) {
		printf("Converting HEIC file to JPG...\n");
		char cmd[2048];

		// Create temporary file in a dir not monitored by inotify
		string sha1 = file->getHashString();
		snprintf(jpgPath, sizeof(jpgPath), "/tmp/%s.jpg", sha1.c_str());
		snprintf(cmd, sizeof(cmd), "tifig -p \"%s\" -q 97 -v %s", inputFile, jpgPath); // meta data will be included
		system(cmd);
		file->writeHashFile();
		delete file;

		// We want to keep original HEIC file. Therefore we use its sha1
		uint8_t hash[FILE_HASH_SIZE];
		memcpy(hash, file->hash, sizeof(hash));

		// Read new version of file
		str3 = strdup(jpgPath);
		file = new File(string("/tmp"), string(basename(str3)));
		file->analyze();
		file->print();

		memcpy(file->hash, hash, sizeof(hash));
	}
	else if (file->type == File::FILE_TYPE_TIF) {
		// Must be converted to JPG
		printf("Converting TIF file to JPG...\n");
		char cmd[2048];

		// Create temporary file in a dir not monitored by inotify
		string sha1 = file->getHashString();
		snprintf(jpgPath, sizeof(jpgPath), "/tmp/%s.jpg", sha1.c_str());
		snprintf(cmd, sizeof(cmd), "convert \"%s\" %s", inputFile, jpgPath); // no meta data
		system(cmd);
		file->writeHashFile();
		delete file;

		// We want to keep original TIF file. Therefore we use its sha1
		uint8_t hash[FILE_HASH_SIZE];
		memcpy(hash, file->hash, sizeof(hash));

		// Read new version of file
		str3 = strdup(jpgPath);
		file = new File(string("/tmp"), string(basename(str3)));
		file->analyze();
		file->print();

		memcpy(file->hash, hash, sizeof(hash));
	}

	if (file->typeIsImage()) {
		printf("------------------------------------------------> image found!\n");
		ReadImageMetaDataTask* readImageMetaDataTask = new ReadImageMetaDataTask(file);
		readImageMetaDataTask->execute();
		delete readImageMetaDataTask;

		// TODO: do not generate sizes that are beyond the src file

		GenerateImageTask* generateImageTask = new GenerateImageTask(file, string(outputDir),
			GenerateImageTask::IMAGE_SIZE_HIGH_RES_2K,
			GenerateImageTask::THUMBNAIL_QUALITY_HIGH);
		generateImageTask->execute();
		delete generateImageTask;
	}

	else if (file->typeIsAnimation()) {
		printf("------------------------------------------------> animation found!\n");

		printf("origAnimation: %d\n", file->origAnimation);

		// Convert still image for thumbnail generation (no high res version)
		char cmd[2048];
		snprintf(cmd, sizeof(cmd), "convert %s /tmp/%s.jpg",
			file->getPath().c_str(), file->getHashString().c_str());
		printf("cmd %s\n", cmd);
		system(cmd);

		char srcImage[1024];
		snprintf(srcImage, sizeof(srcImage), "%s-0.jpg",
			file->getHashString().c_str());
		printf("srcImageP %s\n", srcImage);

		File* file2 = new File(string("/tmp"), string(srcImage));
	    file2->analyze();

	    memcpy(file2->hash, file->hash, sizeof(file->hash));
	    file2->size = file->size;
        file2->fsModifiedTs = file->fsModifiedTs;
        file2->origAnimation = file->origAnimation;

	    ReadImageMetaDataTask* readImageMetaDataTask = new ReadImageMetaDataTask(file2);
		readImageMetaDataTask->execute();
		delete readImageMetaDataTask;

		GenerateImageTask* generateImageTask = new GenerateImageTask(file2, string(outputDir),
			GenerateImageTask::IMAGE_SIZE_THUMBNAIL_LARGE,
			GenerateImageTask::THUMBNAIL_QUALITY_HIGH);
		generateImageTask->execute();
		delete generateImageTask;

		delete file2;

		// Move original (typically low resolution image with loopable animation intact)
		jpgPath[0] = 'y'; // handled below

		// Cleanup
		snprintf(cmd, sizeof(cmd), "rm -f /tmp/%s-*.jpg",
			file->getHashString().c_str());
		printf("cmd %s\n", cmd);
		system(cmd);
	}

	else if (file->typeIsVideo()) {
		ReadVideoMetaDataTask* readVideoMetaDataTask = new ReadVideoMetaDataTask(file);
		readVideoMetaDataTask->execute();
		delete readVideoMetaDataTask;

		GenerateImageTask* generateImageTask = new GenerateImageTask(file, string(outputDir),
			GenerateImageTask::IMAGE_SIZE_HIGH_RES_2K,
			GenerateImageTask::THUMBNAIL_QUALITY_HIGH);
		generateImageTask->execute();
		delete generateImageTask;

		GenerateVideoTask* generateVideoTask = new GenerateVideoTask(file, string(outputDir));
		generateVideoTask->execute();
		delete generateVideoTask;
	}

	else if (file->typeIsAudio()) {
		ReadAudioMetaDataTask* readAudioMetaDataTask = new ReadAudioMetaDataTask(file);
		readAudioMetaDataTask->execute();
		delete readAudioMetaDataTask;

		GenerateAudioTask* generateAudioTask = new GenerateAudioTask(file, string(outputDir));
		generateAudioTask->execute();
		delete generateAudioTask;
	}

	// Move original file, if applicable
	if (jpgPath[0]) {
		FileSystem::moveFile(string(inputFile), string(outputDir) + "/" + file->getHashString());
	}

	delete file;

	sleep(2);

	FreeImage_DeInitialise();

	free(str1);
	free(str2);

	return res;
}

int main(int argc, char* argv[]) {
	int res;

	parseArguments(argc, argv);

	if (helpScreen) {
		printHelp();
		goto exit;
	}

	/* Due to thread safety limitations in external libraries this program
	   uses fork instead of threads to handle concurrency. This is a cleaner
	   way of managing it without inconsistent usage of locks.

	   The program is intended to be started in the file detection mode
	   which scans the file system for files to process. Then it invokes
	   the program again in file processing mode for each file. The level
	   of concurrency is configurable.
	 */

	/****************************************************************************************************
	***************************************** File Detection Mode ***************************************
	****************************************************************************************************/	
	if (!inputFile[0]) {
		res = fileDetectionMode();
	}

	/****************************************************************************************************
	***************************************** File Processing Mode **************************************
	****************************************************************************************************/
	else {
		res = fileProcessingMode();
	}

	if (res != 0) {
		goto error;
	}

exit:
	return EXIT_SUCCESS;
error:
	return EXIT_FAILURE;
}

/*-------------------------------------------------------------------------------------------------------------------*/
