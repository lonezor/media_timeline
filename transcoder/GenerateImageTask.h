#ifndef GENERATE_IMAGE_TASK_H_
#define GENERATE_IMAGE_TASK_H_

#include "Task.h"
#include "File.h"

#include <string>
#include <FreeImage.h>

typedef struct {
	uint32_t size;
	uint32_t quality;
} imageBf_t;

using namespace std;

class GenerateImageTask : Task {

public:

	enum ImageSize {
		IMAGE_SIZE_UNKNOWN             = 0x00,
		IMAGE_SIZE_THUMBNAIL_SMALL     = 0x01, /* 160x120                 */
		IMAGE_SIZE_THUMBNAIL_MEDIUM    = 0x02, /* 320x240                 */
		IMAGE_SIZE_THUMBNAIL_LARGE     = 0x04, /* 640x480                 */
		IMAGE_SIZE_THUMBNAIL_ALL       = 0x07, /* All thumb nail sizes    */
		IMAGE_SIZE_HIGH_RES_1K         = 0x08, /* 1024x768                */
		IMAGE_SIZE_HIGH_RES_2K         = 0x10, /* 2048x1536               */
		IMAGE_SIZE_HIGH_RES_4K         = 0x20, /* 4096x3072               */
		IMAGE_SIZE_HIGH_RES_8K         = 0x40, /* 8192x6144               */
		IMAGE_SIZE_HIGH_RES_ALL        = 0x78, /* All high res sizes      */
		IMAGE_SIZE_LARGEST             = 0x40, /* For backwards traversal */
		IMAGE_SIZE_ALL                 = 0x7f, /* All existing sizes      */
	};

	enum ThumbNailQuality {
		THUMBNAIL_QUALITY_UNKNOWN  = 0x00,
		THUMBNAIL_QUALITY_LOW      = 0x01, /* Optimized PNG: Poor quality that loads extremely quickly */
		THUMBNAIL_QUALITY_HIGH     = 0x02, /* JPEG: High quality with little compression artifacts          */
		THUMBNAIL_QUALITY_ALL      = 0x03, /* All quality settings                                          */
	};

	GenerateImageTask(File* file, string targetDirName, ImageSize sizeBf, ThumbNailQuality qualityBf);

	virtual void execute(void);

	static uint32_t   getNrBitsInBf(uint32_t allBitsSet);
	static string     getImageSizeSuffix(ImageSize size);
	static string     getThumbNailQualitySuffix(ThumbNailQuality quality);
	static string     getThumbNailFileType(ThumbNailQuality quality, bool isVideo);

	bool              hashFileFoundInFs(void);
	bool              hashFileMatchCurrentHash(void);
	bool              allImagesFoundInFs(imageBf_t* missingImages);
	bool              generateImageFiles(imageBf_t images);
	bool              writeHashFile(void);
	string            getHashFilePath(void);
	static string     getImageFileName(File* file, ImageSize size, ThumbNailQuality quality);
	static uint32_t   getImageWidth(ImageSize size);
	static uint32_t   getImageHeight(ImageSize size);

	FREE_IMAGE_FORMAT getFreeImageFormat(File::FileType fileType);
	FREE_IMAGE_FORMAT getFreeImageOutputFormat(ThumbNailQuality quality);
	uint32_t          getFreeImageLoadFlags(File::FileType fileType);
	uint32_t          getFreeImageSaveFlags(ThumbNailQuality quality, ImageSize size);
	uint32_t          getNrQuantizationColors(ThumbNailQuality quality);
	bool              writeOptimizedPng(FIBITMAP* bitmap, ThumbNailQuality quality, string path);


	bool              floatIsAlmostEqualTo(float value, float ref);
	void              readMetaData(FIBITMAP* bitmap, FREE_IMAGE_MDMODEL model, File* file);
	bool              generateImageFiles_imageSrc(imageBf_t images);
	void              generateImageFiles_imageSrc_loadPngFile(File* file, FIBITMAP** bitmap);
	void              generateImageFiles_imageSrc_loadImageFile(File* file, FIBITMAP** bitmap);
	void              generateImageFiles_imageSrc_readMetaData(File* file, FIBITMAP* bitmap);
	void              generateImageFiles_imageSrc_autoRotate(File* file, FIBITMAP** bitmap);
	FIBITMAP*         generateImageFiles_imageSrc_createLetterBoxedThumbNail(FIBITMAP* bitmap, uint32_t width, uint32_t height,
			                                                                  uint32_t thumbNailWidth, uint32_t thumbNailHeight, float aspectRatio);
	bool              generateImageFiles_imageSrc_createNonLetterBoxedImage(File* file, FIBITMAP* bitmap, uint32_t width, float aspectRatio, string targetDirName);
	bool              generateImageFiles_videoSrc(imageBf_t images);
	string            getTimestampFromDuration(float duration);

    File*             file;
    string            targetDirName;
    ImageSize         sizeBf;
    ThumbNailQuality  qualityBf;
};


#endif /* GENERATE_IMAGE_TASK_H_ */
