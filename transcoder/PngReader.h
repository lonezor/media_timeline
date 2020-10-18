#ifndef PNGREADER_H_
#define PNGREADER_H_

#include <string>
#include <png.h>
#include <FreeImage.h>

using namespace std;

class PngReader {
public:

	PngReader(string path);

	FIBITMAP* getBitmap(void);

private:
	string      path;

};




#endif /* PNGREADER_H_ */
