#ifndef IMAGE_H_
#define IMAGE_H_

#include "File.h"
#include "Media.h"
#include "FreeImage.h"

#include <string>

#define ASPECT_RATIO_4_3 ((float)4 / (float)3)

using namespace std;

class Image : Media { /* using screen coordinate system (bottom-up) */
public:
	Image(File* srcFile);
	Image();
	~Image();

	bool   loadImage();
	bool   writeImage(string path);
	bool   writeIndexedPng(string path);

	void autoRotate(void);
	Image* createLetterBoxedImage(uint32_t srcWidth,    uint32_t srcHeight,
                                  uint32_t targetWidth, uint32_t targetHeight);
	Image* createRescaledImage(uint32_t targetWidth, uint32_t targetHeight);
	Image* createCroppedImage(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);
	void   colorQuantizate(void);
	Image* createFilmReelImage(void);
	void   drawRectangle(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint8_t red, uint8_t green, uint8_t blue);

	uint32_t       width;
	uint32_t       height;
	FIBITMAP*      bitmap;

private:
	FREE_IMAGE_FORMAT getFreeImageFormat(File::FileType fileType);
	uint32_t          getFreeImageLoadFlags(File::FileType fileType);

	string         srcFilePath;
	File::FileType fileType;
	bool           quantizationPerformed;
	bool           srcImagLoaded;

};




#endif /* IMAGE_H_ */
