#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <string>
#include <stdint.h>
#include <list>
#include <map>

#include "Directory.h"

using namespace std;

class FileSystem {

public:

	enum FileSystemEvent {
		FILE_SYSTEM_EVENT_UNKNOWN = 0,
		FILE_SYSTEM_EVENT_CREATE  = 1,
		FILE_SYSTEM_EVENT_MODIFY  = 2,
		FILE_SYSTEM_EVENT_DELETE  = 3,
	};

	typedef struct {
        File*           file;
        Directory*      affectedDir;
		uint32_t        inotifyModifiedTs; /* Timestamp of last inotify update (possibly partial file write) */
		FileSystemEvent inotifyInitialEvent;
	} fileEventData_t;

	typedef void fsFileEventCb_t(FileSystem::FileSystemEvent event, uint32_t timestamp, File* file, Directory* affectedDir, void* userData);
	typedef void fsDirectoryEventCb_t(FileSystem::FileSystemEvent event, uint32_t timestamp, Directory* directory, void* userData);

	FileSystem();

	void scan(Directory* dir);
	void addDirWatchers(int inotifyFd);
	void removeDirWatchers(int inotifyFd);
	void registerFileEventCallback(fsFileEventCb_t* cb, void* userData);
	void registerDirectoryEventCallback(fsDirectoryEventCb_t* cb, void* userData);
	void fsEventDataAvailable(struct inotify_event* event);
	void tick(void);
	static bool fileExists(string path);

	list<Directory*>                   dirList;
	std::map<int,Directory*>           dirWatchMap;    /* key: watch descriptor */
	std::map<string,Directory*>        dirPathMap;     /* key: user path        */
	std::map<string,fileEventData_t*>  fileEventMap;   /* key: fileEventData_t  */
	std::map<string,File*>             fileHashMap;    /* key: sha1 hash string */
	fsFileEventCb_t*                   fileCb;
	void*                              fileCbUserData;
	fsDirectoryEventCb_t*              dirCb;
	void*                              dirCbUserData;
	int                                inotifyFd;
	string                             baseDir;

	static string convertFileSystemEventToString(FileSystemEvent fsEvent);
	static bool   makedir(string path); /* will recurse if needed */
	static bool   removeFile(string path);
	static bool   moveFile(string srcPath, string dstPath);


};




#endif /* FILESYSTEM_H_ */
