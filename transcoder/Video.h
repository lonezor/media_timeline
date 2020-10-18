#ifndef VIDEO_H_
#define VIDEO_H_

#include "File.h"
#include "Media.h"
#include "Image.h"

#include <string>


using namespace std;

class Video : Media {
public:

	Video(File* file);

	void   readMetaData(void);
	Image* getImage(string timestamp); /* format "HH:MM:SS.msec" */
	bool   encode(string targetDir, string baseFileName, Resolution resolution);

	File* file;
	uint32_t width;
	uint32_t height;
	double duration;
};




#endif /* VIDEO_H_ */
