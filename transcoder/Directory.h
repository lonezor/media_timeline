#ifndef DIRECTORY_H_
#define DIRECTORY_H_

#include <string>
#include <stdint.h>
#include <list>
#include <map>
#include "File.h"

using namespace std;

class Directory {

public:

	Directory(string path);

	void setInotifyWatchRef(int wd);
    void clearInotifyWatchRef(void);
    string getBaseName(void);
    string getRelativePath(string base);

	string                      path;
    list<File*>                 fileList;
    list<Directory*>            dirList;
	int                         inotifyWatchRef;
	std::map<string,Directory*> dirWatchMap; /* Map used to lookup sub directories in dir; key: path */

private:

};


#endif /* DIRECTORY_H_ */
