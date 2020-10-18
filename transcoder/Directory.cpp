#include "Directory.h"
#include <libgen.h>


Directory::Directory(string path) {
	this->path = path;
	this->inotifyWatchRef = -1;
}

void Directory::setInotifyWatchRef(int wd) {

}

void Directory::clearInotifyWatchRef(void) {

}

string Directory::getBaseName() {
	string tmpString = string(path.c_str());

	return string(basename((char*)tmpString.c_str()));
}

string Directory::getRelativePath(string base) {
	string tmpPath = string(path.c_str()); /* deep copy */
	return "/" + tmpPath.erase(0, base.size()+1);
}
