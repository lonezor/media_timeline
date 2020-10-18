#include "FileSystem.h"
#include "general.h"

#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <errno.h>


/*-------------------------------------------------------------------------------------------------------------------*/

FileSystem::FileSystem() {
	this->fileCb         = NULL;
	this->fileCbUserData = NULL;
	this->dirCb          = NULL;
	this->dirCbUserData  = NULL;
	this->inotifyFd      = -1;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool FileSystem::fileExists(string path) {
    struct stat sb;
	bool found;
	int result;

    result = stat(path.c_str(), &sb);
    found = (result == 0);

    return found;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::scan(Directory* dir) {
	struct dirent **namelist;
	int i,n;
	string dirPath = dir->path;
	string tmpPath = string(dirPath.c_str()); /* deep copy */
	string relativePath = "/" + tmpPath.erase(0, baseDir.size()+1);

	printf("scan relative path %s\n", relativePath.c_str());

    dirList.push_back(dir);
    dirPathMap[relativePath] = dir;

	printf("scanning %s\n", dirPath.c_str());

	n = scandir(dirPath.c_str(), &namelist, 0, alphasort);
	if (n < 0)  {
		perror("scandir");
		return;
	}

	for (i = 0; i < n; i++) {
		string fileName = string(namelist[i]->d_name);
		uint8_t type    = namelist[i]->d_type;
		string path = dirPath + "/" + fileName;
		size_t pos;

		if (fileName == "." ||
			fileName == "..") {
			free(namelist[i]);
			continue;
		}

		if (type == DT_REG) {
			File* file = new File(dirPath, fileName);
			file->analyze();
			file->print();

			dir->fileList.push_back(file);

			if (fileCb) {
				fileCb(FILE_SYSTEM_EVENT_CREATE,
					   time(NULL),
					   file,
					   NULL,
					   fileCbUserData);
				}
			delete file;
		}
		else if (type == DT_DIR) {
			Directory* directory = new Directory(path);
			scan(directory);
			dir->dirList.push_back(directory);
		}
		free(namelist[i]);
	}
	free(namelist);
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::addDirWatchers(int inotifyFd) {
	this->inotifyFd = inotifyFd;

	for(list<Directory*>::iterator it = dirList.begin(); it != dirList.end(); ++it) {
		int wd;
		Directory* dir = *it;

		wd = inotify_add_watch(inotifyFd, dir->path.c_str(), IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE);
		printf("watching %s for fs updates wd=%d\n", dir->path.c_str(), wd);

		dirWatchMap[wd] = dir;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::removeDirWatchers(int inotifyFd) {
	// TODO
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::registerFileEventCallback(fsFileEventCb_t* cb, void* userData) {
	fileCb = cb;
	fileCbUserData = userData;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::registerDirectoryEventCallback(fsDirectoryEventCb_t* cb, void* userData) {
	dirCb = cb;
	dirCbUserData = userData;
}

/*-------------------------------------------------------------------------------------------------------------------*/

string FileSystem::convertFileSystemEventToString(FileSystemEvent fsEvent) {
	switch (fsEvent) {
	case FILE_SYSTEM_EVENT_CREATE:
		return "FileSystemEventCreate";
	case FILE_SYSTEM_EVENT_MODIFY:
		return "FileSystemEventModify";
	case FILE_SYSTEM_EVENT_DELETE:
		return "FileSystemEventDelete";
	case FILE_SYSTEM_EVENT_UNKNOWN:
	default:
		return "FileSystemEventunknown";
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void FileSystem::fsEventDataAvailable(struct inotify_event* event) {
    Directory* affectedDir;
	FileSystemEvent fsEvent = FILE_SYSTEM_EVENT_UNKNOWN;
	File* file;
	string path;

	if (dirWatchMap.count(event->wd) == 0) {
		printf("error: unknown wd %d\n", event->wd);
		return;
	}

	/* Filter out events with unsupported mask values */
	if(!((event->mask & IN_CLOSE_WRITE) ||
		 (event->mask & IN_MOVED_TO)    || 
		 (event->mask & IN_DELETE))) {
		return;
	}

	affectedDir = dirWatchMap[event->wd];
	path = affectedDir->path + "/" + string(event->name);

	if ((event->mask & IN_CLOSE_WRITE) || (event->mask & IN_MOVED_TO)) {
		fsEvent = FILE_SYSTEM_EVENT_CREATE;
	}
	else if (event->mask & IN_DELETE) {
		fsEvent = FILE_SYSTEM_EVENT_DELETE;
	}

	if (fileCb) {
		file = new File(affectedDir->path, string(event->name));
		file->analyze();
		fileCb(fsEvent, time(NULL), file, NULL, fileCbUserData);
		delete file;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/


bool FileSystem::makedir(string path) {
	bool         dirCreated = TRUE;
	int          result;
	int          mode = S_IRWXU | S_IRWXG | S_IRWXO;
	char*        p = NULL;
	char*        token;
	string       dir = "/";
	list<string> dirs;

    /* First try the most common cases */
	result = mkdir(path.c_str(), mode);
	if (result == 0) {
		return dirCreated;
	} else if (result == -1 && errno == EEXIST) {
		return dirCreated;
	}

	/* Create full directory path including parent dirs */
	token = strtok_r((char*)path.c_str(), "/", &p);
	dir += string(token);
    dirs.push_back(dir);

	while ((token = strtok_r((char*)NULL, "/", &p)) != NULL) {
		dir += "/" + string(token);
		dirs.push_back(dir);
	}

	for(list<string>::iterator it = dirs.begin(); it != dirs.end(); ++it) {
		string d = *it;

		result = mkdir(d.c_str(), mode);
		if (result == -1 && errno != EEXIST) {
			dirCreated = FALSE;
		}
	}

	return dirCreated;
}

bool FileSystem::removeFile(string path) {
	int res;

	res = unlink(path.c_str());

	return (res == 0);
}

bool FileSystem::moveFile(string srcPath, string dstPath) {
	return true;
	int res;
	char* d;
	string p = string(dstPath.c_str());
	string dName = "";

	d = dirname((char*)p.c_str());
	dName = string(d);

	makedir(dName);

	res = rename(srcPath.c_str(), dstPath.c_str());

	return (res == 0);
}

/*-------------------------------------------------------------------------------------------------------------------*/

