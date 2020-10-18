#if 0
#ifndef WORKERTHREAD_H_
#define WORKERTHREAD_H_

#include "SharedThreadData.h"
#include "WorkerThread.h"
#include "GenerateImageTask.h"
#include "Html.h"
#include <stdint.h>
#include <vector>
#include <map>

typedef void* threadFunction_t(void* data);

typedef struct {
	void*             workerThread;
	SharedThreadData* sharedThreadData;
	Options*          options;
	FileSystem*       srcFileSystem;
	FileSystem*       dstFileSystem;
} context_t;

typedef struct {
	string dir;
	string file;
	string size;
	string quality;
	string screenWidth;
	string screenHeight;
	string availScreenWidth;
	string availScreenHeight;
	string colorDepth;
	string resolution;
} pageRequest_t;

/* WorkerThread is controlled from creating thread */
class WorkerThread {
    public:
        WorkerThread(SharedThreadData* sharedThreadData, int threadIndex, bool isMaster);
        ~WorkerThread();

        bool      start();
        void      masterThreadMain(SharedThreadData* sharedThreadData);
        void      slaveThreadMain(SharedThreadData* sharedThreadData);
        int32_t   getTid(void);

        pthread_t          threadId;
        SharedThreadData*  sharedThreadData;
        int                threadIndex; /* Used for load distribution */
        bool               isRunning;
        bool               isMaster;

        void addGenerateSrcFileHashTaskToQueue(context_t* ctx, File* file);
        void addReadImageMetaDataTaskToQueue(context_t* ctx, File* file);
        void addReadVideoMetaDataTaskToQueue(context_t* ctx, File* file);
        void addGenerateImageTaskToQueue(context_t* ctx, File* file);
        void addGenerateVideoTaskToQueue(context_t* ctx, File* file);
        bool metaDataThreadHasEmptyQueue(void);
        bool generalPurposeThreadsHaveAllEmptyQueues(void);
        bool allWorkerThreadsHaveEmptyQueues(void);
        bool allWorkerThreadsIdling(void);

        string handleRequest(context_t* ctx, string request);
        void nextRoundRobinIndex(context_t* ctx, bool timeConsumingTask);
        vector<string> split(string str, string separator);
        uint32_t subStringCount(string str, string subStr);
        string   handlePageRequest(context_t* ctx, pageRequest_t* pReq);
        string   handlePageRequest_gallery(context_t* ctx, pageRequest_t* pReq);
        string   handlePageRequest_file(context_t* ctx, pageRequest_t* pReq);
        string   handlePageRequest_file_image(context_t* ctx, pageRequest_t* pReq, File* file);
        string   handlePageRequest_file_video(context_t* ctx, pageRequest_t* pReq, File* file);
        void     handlePageRequest_header(context_t* ctx, pageRequest_t* pReq, Html* h);
        void     handlePageRequest_body(context_t* ctx, pageRequest_t* pReq, Html* h);
        void     handlePageRequest_generateDirectorySelector(context_t* ctx, pageRequest_t* pReq, Html* h);

        string   handleAuthRequest(context_t* ctx, string username, string password);
        string   handleDebugShowWorkQueues(context_t* ctx);
        string   handleDebugShowCounters(context_t* ctx);
        string   handleDebugShowRaidStatus(context_t* ctx);
        string   handleDebugShowProcessLoad(context_t* ctx);
		string   handleDebug(context_t* ctx);
        int      setupListeningSocket(void);
        int      setupInotify(void);
        uint32_t stringToUint32(string str);
        string   uint32ToString(uint32_t value);

        string   getHashFilePath(context_t* ctx, File* file);
        string   getHashStringFromFilePath(context_t* ctx, string path);
        bool     hashFileFoundInFs(context_t* ctx, File* file);
        uint32_t getNrBitsInBf(uint32_t allBitsSet);
        string   getImageFileName(GenerateImageTask::ImageSize size, GenerateImageTask::ThumbNailQuality quality);
        bool     allImagesFoundInFs(context_t* ctx, File* file);
        bool     allVideosFoundinFs(context_t* ctx, File* file);
        bool     fileFoundinFs(string path);
        bool     hashFileMatchCurrentHash(context_t* ctx, File* file);
        void     relocateDstFiles(context_t* ctx, File* oldDstHashFile, File* srcFile);
        void     removeDstFiles(context_t* ctx, File* oldDstHashFile);
};

#endif /* WORKERTHREAD_H_ */
#endif