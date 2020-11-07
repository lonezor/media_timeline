#include "GenerateImageTask.h"
#include "FileSystem.h"
#include "PngReader.h"
#include "PngWriter.h"
#include "general.h"
#include "ShellCmd.h"
#include "Image.h"
#include "Video.h"
#include "Common.h"

#include <mhash.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <list>
#include <errno.h>
#include <cmath>

#include "FreeImage.h"
#include "libimagequant.h"

#define ASPECT_RATIO_4_3 ((float)4 / (float)3)
#define ASPECT_RATIO_5_4 ((float)5 / (float)4)
#define ASPECT_RATIO_16_9 ((float)16 / (float)9)

/*-------------------------------------------------------------------------------------------------------------------*/

GenerateImageTask::GenerateImageTask(File* file, string targetDirName,
		ImageSize sizeBf, ThumbNailQuality qualityBf) {
	this->file          = file;
	this->targetDirName = targetDirName;
	this->sizeBf        = sizeBf;
	this->qualityBf     = qualityBf;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void GenerateImageTask::execute() {
	imageBf_t missingImages;
	imageBf_t images;
    bool generateHashFile = FALSE;
    bool result;

	printf("[%s GenerateImageTask] %s %s %d %d\n", __FUNCTION__,
		   file->getPath().c_str(), targetDirName.c_str(), sizeBf, qualityBf);

	result = FileSystem::makedir(targetDirName);
	if (!result) {
		printf("[%s] failed to create directory %s\n", __FUNCTION__, targetDirName.c_str());
		return;
	}

	/* Find out if image files need to be created. The persistent hash file
	 * describes which source file was used.
	 */
    if (!hashFileFoundInFs() || !hashFileMatchCurrentHash()) {
    	images.size    = IMAGE_SIZE_HIGH_RES_2K;
    	images.quality = THUMBNAIL_QUALITY_HIGH;
    	generateHashFile = TRUE;
	// else if (!allImagesFoundInFs(&missingImages)) {
   // 	images = missingImages;
    } else { // nothing to do
    	if (file->typeIsImage()) {
    		file->completelyProcessedImages = TRUE;
    		file->completelyProcessedImagesTs = (uint32_t)time(NULL);
    		//FileSystem::removeFile(file->getPath());
    	}
    	return;
    }

	generateImageFiles(images);

	if (generateHashFile) {
		writeHashFile();
	}

	file->completelyProcessedImages = TRUE;
	file->completelyProcessedImagesTs = (uint32_t)time(NULL);
}

/*-------------------------------------------------------------------------------------------------------------------*/

string GenerateImageTask::getHashFilePath() {
	return file->getPath() + ".info";
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::writeHashFile() {
	FILE*     dstFile;
	uint32_t  i;
	string    dstPath = getHashFilePath();

	dstFile = fopen(dstPath.c_str(), "w");
	if (dstFile == NULL) {
		printf("[%s] failed to open destination file %s\n", __FUNCTION__, dstPath.c_str());
		return FALSE;
	}

	fprintf(dstFile, "sha1: ");
	for(i=0; i<sizeof(file->hash);i++) {
		fprintf(dstFile, "%02x", file->hash[i]);
	}
	fprintf(dstFile, "\n");

	fclose(dstFile);

	return TRUE;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::hashFileFoundInFs() {
    struct stat sb;
	bool found;
	int result;

    result = stat(getHashFilePath().c_str(), &sb);
    found = (result == 0);

    if (!found) {
    	printf("%s: hash not found in fs\n", getHashFilePath().c_str());
    }

    return found;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::allImagesFoundInFs(imageBf_t* missingImages) {
	uint32_t i;
	uint32_t j;
	uint32_t size;
	uint32_t quality;
	struct stat sb;
	bool     found = TRUE;
	int      result;

	memset(missingImages, 0, sizeof(imageBf_t));

	for(i=0 , size=0x01; i < getNrBitsInBf(IMAGE_SIZE_ALL); i++ , size <<= 1) {
		for(j=0, quality=0x01; j < getNrBitsInBf(THUMBNAIL_QUALITY_ALL); j++, quality <<= 1) {

			if (size > IMAGE_SIZE_THUMBNAIL_ALL && quality == THUMBNAIL_QUALITY_LOW) {
				continue;
			}

			if (file->typeIsVideo() && size > IMAGE_SIZE_THUMBNAIL_ALL) {
				continue;
			}

			string path = targetDirName + "/";
			path += getImageFileName(file, (ImageSize)size, (ThumbNailQuality)quality);

			result = stat(path.c_str(), &sb);
			if (result != 0) {
				missingImages->size    |= size;
				missingImages->quality |= quality;
				found = FALSE;
				printf("Image %s not found. It will be generated (size 0x%02x quality 0x%02x).\n",
						path.c_str(),
						size,
						quality);
			}
		}
	}

    return found;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::hashFileMatchCurrentHash() {
    #define PREFIX_LEN 6
	FILE*     hashFile;
    uint32_t  i;
	uint32_t  fileSize;
    uint32_t  expectedFileSize = PREFIX_LEN + (sizeof(file->hash)*2) + 1;
	string    path = getHashFilePath();
	uint8_t*  persistentHash;
	uint8_t*  pos;
	size_t    result;
	bool      identical = FALSE;

	hashFile = fopen(path.c_str(), "r");
	if (hashFile == NULL) {
		printf("unable to read %s\n", path.c_str());
		return identical;
	}

	fseek(hashFile, 0, SEEK_END);
	fileSize = ftell(hashFile);
	rewind(hashFile);

	if (fileSize != expectedFileSize) {
		printf("incorrect length of %s (real size %u but expected %u)\n", path.c_str(), fileSize, expectedFileSize);
		return identical;
	}

	persistentHash = (uint8_t*) malloc(fileSize);
    assert(persistentHash);

    result = fread(persistentHash, 1, fileSize, hashFile);
    if (result != fileSize) {
    	printf("could not read all bytes in %s\n", path.c_str());
    	free(persistentHash);
    	fclose(hashFile);
    	return identical;
    }

    pos = persistentHash + PREFIX_LEN;
    identical = TRUE;
    for(i=0; i<sizeof(file->hash); i++) {
    	char str[3];
    	uint8_t byte = 0;

    	str[0] = *(pos);
    	str[1] = *(pos+1);
    	str[2] = 0;

    	byte = strtol(str, NULL, 16);

    	//printf("%s index %u byte %02x hash %02x --> %d\n", path.c_str(), i, byte, file->hash[i], (byte == file->hash[i]));
    	identical &= (byte == file->hash[i]);

    	pos += 2;
    }

    if (!identical) {
    	printf("%s: hash file not mathing current hash\n", file->getPath().c_str());
    }

   	free(persistentHash);
   	fclose(hashFile);

	return identical;
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t GenerateImageTask::getNrBitsInBf(uint32_t allBitsSet) {
	uint32_t nrOfBits = 0;

	while(allBitsSet > 0) {
		allBitsSet >>= 1;
		nrOfBits++;
	}

	return nrOfBits;
}

/*-------------------------------------------------------------------------------------------------------------------*/

FREE_IMAGE_FORMAT GenerateImageTask::getFreeImageFormat(File::FileType fileType) {
	switch (fileType) {
	    case File::FILE_TYPE_JPG:
	    	return FIF_JPEG;
	    case File::FILE_TYPE_GIF:
	    	return FIF_GIF;
	    case File::FILE_TYPE_PNG:
	    	return FIF_PNG;
	    case File::FILE_TYPE_UNKNOWN:
	    default:
		    return FIF_UNKNOWN;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

FREE_IMAGE_FORMAT GenerateImageTask::getFreeImageOutputFormat(ThumbNailQuality quality) {
	if (quality == THUMBNAIL_QUALITY_HIGH) {
		return FIF_JPEG;
	} else {
		return FIF_PNG;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t GenerateImageTask::getFreeImageLoadFlags(File::FileType fileType) {
	uint32_t flags = 0;

	switch (fileType) {
	    case File::FILE_TYPE_JPG:
	    	flags = JPEG_ACCURATE;
	    	break;
	    default:
	    	flags = 0;
	    	break;
	}

	return flags;
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t GenerateImageTask::getFreeImageSaveFlags(ThumbNailQuality quality, ImageSize size) {
	uint32_t flags = JPEG_OPTIMIZE | JPEG_PROGRESSIVE;

	if (size == IMAGE_SIZE_HIGH_RES_1K ||
		size == IMAGE_SIZE_HIGH_RES_2K ||
	    size == IMAGE_SIZE_HIGH_RES_4K ||
	    size == IMAGE_SIZE_HIGH_RES_8K) {
		flags |= 95;
	}
	else if (quality == THUMBNAIL_QUALITY_HIGH) {
		flags = 90 | JPEG_OPTIMIZE | JPEG_PROGRESSIVE;
	} else {
		flags = PNG_Z_BEST_SPEED; /* only for debug. PNG is optimized separately */
	}

	return flags;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::writeOptimizedPng(FIBITMAP* bitmap, ThumbNailQuality quality, string path) {
	uint32_t           x;
	uint32_t           y;
	uint32_t           width;
	uint32_t           height;
	BYTE*              scanLine;
	BYTE**             scanLines;
	liq_attr*          attr;
	liq_image*         img;
	liq_result*        res;
	const liq_palette* palette;
	uint32_t           error;
	uint8_t**          scanLinesIndexedColor;
	uint32_t           nrOfColors;
	uint32_t           bitDepth;
	PngWriter*         pngWriter = new PngWriter(path);
	bool               result;
	double             paletteError;

	bitDepth = FreeImage_GetBPP(bitmap);

	/* Expect RGBA */
	if (bitDepth != 32) {
		printf("[%s] bad pixel depth (%u)!\n", __FUNCTION__, bitDepth);
		return FALSE;
	}

	width  = FreeImage_GetWidth(bitmap);
	height = FreeImage_GetHeight(bitmap);

	nrOfColors = getNrQuantizationColors(quality);

	scanLines = (BYTE**) malloc(sizeof(BYTE*) * height);
	assert(scanLines);

	scanLinesIndexedColor = (uint8_t**) malloc(sizeof(uint8_t*) * height);
	assert(scanLinesIndexedColor);

	for(y=0; y < height; y++) {
		/* The scan lines are stored according to the screen     *
		 * coordinate system (rendering from top to bottom).     *
		 * So we read them backwards to be compatible with       *
		 * how the PNG quantization library operates on images.  */
		scanLine = FreeImage_GetScanLine(bitmap, height-1-y);
		scanLines[y] = scanLine;

		/* FreeImage stores pixel data with platform awareness.   *
		 * Something not obvious is that the endian of the        *
		 * machine itself cannot say which order to use. Some     *
		 * platforms like MacOSX does not follow the convention.  *
		 *                                                        *
		 * For Linux x86 the source image byte order is BGRA      *
		 * instead of RGBA. The PNG quantization library          *
		 * expects RGBA order so this needs to be adjusted.       */
		for(x=0; x < width; x++) {
			uint8_t* px = scanLine + (4*x);
			uint8_t pxData[4];
			memcpy(pxData, px, 4);
			*(px+0) = pxData[FI_RGBA_RED];
			*(px+1) = pxData[FI_RGBA_GREEN];
			*(px+2) = pxData[FI_RGBA_BLUE];
			*(px+3) = pxData[FI_RGBA_ALPHA];
		}

		scanLinesIndexedColor[y] = (uint8_t*) malloc(sizeof(uint8_t) * width); /* 8 bit color index per pixel */
		assert(scanLinesIndexedColor[y]);
	} /* end of scan line loop */

	attr = liq_attr_create();
	liq_set_max_colors(attr, nrOfColors);

	img = liq_image_create_rgba_rows(attr, (void**)scanLines, width, height, 0);
    res = liq_quantize_image(attr,img);

    liq_set_dithering_level(res, 0.95F); // 0: fast non-dithered remapping. Otherwise a variation of Floyd-Steinberg error diffusion is used.

    error = liq_write_remapped_image_rows(res, img, scanLinesIndexedColor);
    if (error != LIQ_OK) {
    	printf("error converting bitmap\n");
    } else {
    	printf("remapped OK\n");
    }

    paletteError = liq_get_quantization_error(res);
    if (paletteError >= 0) {
    	int q = liq_get_quantization_quality(res);
    	printf("quantization results: MSE=%.3f, Q=%d\n", paletteError, q);
    }

    palette = liq_get_palette(res);

    result = pngWriter->write(scanLinesIndexedColor, palette, width, height);

    liq_image_destroy(img);
    liq_attr_destroy(attr);
    liq_result_destroy(res);
	free(scanLines);

	for(y=0; y < height; y++) {
		free(scanLinesIndexedColor[y]);
	}
	free(scanLinesIndexedColor);

	delete pngWriter;

    return result;
}

/*-------------------------------------------------------------------------------------------------------------------*/

FIBITMAP* GenerateImageTask::generateImageFiles_imageSrc_createLetterBoxedThumbNail(FIBITMAP* bitmap, uint32_t width, uint32_t height,
		                                                    uint32_t thumbNailWidth, uint32_t thumbNailHeight, float aspectRatio) {

	FIBITMAP* thumbNailBitmap;
	FIBITMAP* letterboxBitmap = NULL;
	RGBQUAD   bgColor;
	bool      result;

	thumbNailBitmap = FreeImage_Allocate(thumbNailWidth,
			                             thumbNailHeight,
			                             24);
	assert(thumbNailBitmap);

	/* Set letterbox background to black */
	memset(&bgColor, 0x00, sizeof(bgColor));
	FreeImage_FillBackground(thumbNailBitmap, &bgColor, 0);

	/* Landscape orientation */
	if (aspectRatio > ASPECT_RATIO_4_3) {
		float top;
		float letterBoxHeight;

		letterBoxHeight = ceil((float)thumbNailWidth / aspectRatio);
		if (letterBoxHeight > thumbNailHeight) {
			letterBoxHeight = thumbNailHeight;
		}

		printf("Landscape orientation, aspect ratio %f %dx%d letterbox height %d --> %d x %d\n",
               aspectRatio, width, height, (int)letterBoxHeight, thumbNailWidth, thumbNailHeight);

		letterboxBitmap = FreeImage_Rescale(bitmap,
				                            thumbNailWidth,
				                            (int)letterBoxHeight,
											FILTER_LANCZOS3);
		if (thumbNailBitmap == NULL) {
			printf("[%s] unable to rescale letter boxed aspect ratio thumb nail %s\n", __FUNCTION__,
                  file->getPath().c_str());
			return NULL;
		}

		top = (thumbNailHeight - letterBoxHeight) / 2;

		result = FreeImage_Paste(thumbNailBitmap, letterboxBitmap, 0, (int)top, 256);
		if (!result) {
			printf("[%s] unable to paste landscape create letter boxed thumb nail %s (top=%f)\n",
                   __FUNCTION__, file->getPath().c_str(), top);
			FreeImage_Save(FIF_JPEG, letterboxBitmap, "/tmp/error_letterbox.jpg", 0);
			FreeImage_Save(FIF_JPEG, thumbNailBitmap, "/tmp/error_thumbnail.jpg", 0);
			FreeImage_Unload(letterboxBitmap);
			FreeImage_Unload(thumbNailBitmap);
			return NULL;
		}
	} else { /* Portrait orientation */
		float left;
		float letterBoxWidth;

		letterBoxWidth = ceil((float)thumbNailHeight * aspectRatio);

		printf("portrait orientation, aspect ratio %f %dx%d letterbox width %d --> %d x %d\n",
				aspectRatio, width, height, (int)letterBoxWidth, thumbNailWidth, thumbNailHeight);

		letterboxBitmap = FreeImage_Rescale(bitmap,
				                            (int)letterBoxWidth,
				                            thumbNailHeight,
				                            FILTER_LANCZOS3);
		if (thumbNailBitmap == NULL) {
			printf("[%s] unable to re-scale letter boxed aspect ratio thumb nail %s\n",
                   __FUNCTION__, file->getPath().c_str());
			return NULL;
		}

		left = (thumbNailWidth - letterBoxWidth) / 2;

		result = FreeImage_Paste(thumbNailBitmap,
				                 letterboxBitmap,
								 (int)left, 0, 256);
		if (!result) {
			printf("[%s] unable to paste portrait letter boxed thumb nail %s (left=%f)\n",
					__FUNCTION__, file->getPath().c_str(), left);
			FreeImage_Save(FIF_JPEG, letterboxBitmap, "/tmp/error_letterbox.jpg", 0);
			FreeImage_Save(FIF_JPEG, thumbNailBitmap, "/tmp/error_thumbnail.jpg", 0);
			FreeImage_Unload(letterboxBitmap);
			FreeImage_Unload(thumbNailBitmap);
			return NULL;
		}
	}

	FreeImage_Unload(letterboxBitmap);

    return thumbNailBitmap;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void GenerateImageTask::readMetaData(FIBITMAP* bitmap, FREE_IMAGE_MDMODEL model, File* file) {
	uint32_t count;

	count = (uint32_t)FreeImage_GetMetadataCount(model, bitmap);
	if (count > 0) {
		FITAG* tag;
		FIMETADATA* metaData;

		metaData = FreeImage_FindFirstMetadata(model, bitmap, &tag);

		do {
			const char* tagKey;
			const char* tagValue;
			tagKey = FreeImage_GetTagKey(tag);

			/* TODO: FreeImage_TagToString is not thread safe!!!!!! */
			tagValue = FreeImage_TagToString(model, tag);

			file->addMetaData(string(tagKey), string(tagValue));
		} while (FreeImage_FindNextMetadata(metaData,&tag));
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void GenerateImageTask::generateImageFiles_imageSrc_readMetaData(File* file, FIBITMAP* bitmap) {
	printf("[%s] enter\n", __FUNCTION__);
	//readMetaData(bitmap, FIMD_EXIF_MAIN, file);
	//readMetaData(bitmap, FIMD_EXIF_EXIF, file);
//	readMetaData(bitmap, FIMD_EXIF_GPS, file);
	//readMetaData(bitmap, FIMD_EXIF_MAKERNOTE, file);

	//readMetaData(bitmap, FIMD_EXIF_INTEROP, file);
}

void GenerateImageTask::generateImageFiles_imageSrc_autoRotate(File* file, FIBITMAP** bitmap) {
	FIBITMAP* rotatedBitmap;
	string    orientation;

	/* Auto rotate image if needed, starting with most common case */
	orientation = file->getMetaData("Exif.Orientation");
	file->imageRotationRequired = TRUE; /* easier to disable than having it in almost all cases below */

	if (orientation == "top, left side") {
		printf("no need to rotate\n");
		file->imageRotationRequired = FALSE;
	} else if (orientation == "") { /* often the case with non JPEG sources */
		printf("no orientation data available.\n");
		file->imageRotationRequired = FALSE;
	}
	else if (orientation == "left side, bottom") {
		printf("rotating 90 degrees\n");
		rotatedBitmap = FreeImage_Rotate(*bitmap, 90);
		FreeImage_Unload(*bitmap);
		*bitmap = rotatedBitmap;
	} else if (orientation == "right side, bottom") {
		printf("rotating -90 degrees and flip vertical\n");
		rotatedBitmap = FreeImage_Rotate(*bitmap, -90);
		FreeImage_Unload(*bitmap);
		*bitmap = rotatedBitmap;
		FreeImage_FlipVertical(*bitmap);
	} else if (orientation == "right side, top") {
		printf("rotating -90 degrees\n");
		rotatedBitmap = FreeImage_Rotate(*bitmap, -90);
		FreeImage_Unload(*bitmap);
		*bitmap = rotatedBitmap;
	} else if (orientation == "left side, top") {
		printf("rotating 90 degrees and flip vertical\n");
		rotatedBitmap = FreeImage_Rotate(*bitmap, 90);
		FreeImage_Unload(*bitmap);
		*bitmap = rotatedBitmap;
		FreeImage_FlipVertical(*bitmap);
	} else if (orientation == "bottom, left side") {
		printf("flip vertical\n");
		FreeImage_FlipVertical(*bitmap);
	} else if (orientation == "bottom, right side") {
		printf("rotating 180 degrees\n");
		rotatedBitmap = FreeImage_Rotate(*bitmap, 180);
		FreeImage_Unload(*bitmap);
		*bitmap = rotatedBitmap;
	} else if (orientation == "top, right side") {
		printf("flip horizontal\n");
		FreeImage_FlipHorizontal(*bitmap);
	}
}

void GenerateImageTask::generateImageFiles_imageSrc_loadPngFile(File* file, FIBITMAP** bitmap) {
	PngReader* pngReader = new PngReader(file->getPath());
	*bitmap = pngReader->getBitmap();
	delete pngReader;
}

void GenerateImageTask::generateImageFiles_imageSrc_loadImageFile(File* file, FIBITMAP** bitmap) {
	*bitmap = FreeImage_Load(getFreeImageFormat(file->type),
				             file->getPath().c_str(),
				           	getFreeImageLoadFlags(file->type));
}

bool GenerateImageTask::generateImageFiles_imageSrc_createNonLetterBoxedImage(File* file, FIBITMAP* bitmap, uint32_t width, float aspectRatio, string targetDirName) {
	return true;
	FIBITMAP* scaledNonLetterBoxedBitmap;
	FIBITMAP* tmpBitmap;
	string    path;
	float     newWidth = 8192; // scale down to highest thumbnail resolution if very large
	float     newHeight;
	bool saveResult;
	bool result = TRUE;

    // never upscale
	if (width < newWidth) {
		newWidth = width;
	}

	newHeight = ceil(newWidth / aspectRatio);

	scaledNonLetterBoxedBitmap = FreeImage_Rescale(bitmap,
			(uint32_t) newWidth,
			(uint32_t) newHeight,
			FILTER_LANCZOS3);

	/* TODO: Temporary fix for getting rid of rotation metadata. Not
	 * a real solution (just the same as letterbox setup).... redo this later
	 * in a proper way
	 */
	tmpBitmap = FreeImage_Allocate((uint32_t) newWidth,
			                       (uint32_t) newHeight,
				                   24);
	FreeImage_Paste(tmpBitmap, scaledNonLetterBoxedBitmap, 0, 0, 256);

	path = targetDirName + "/";
	path += file->getHashString() + "_dl.jpg";

	saveResult = FreeImage_Save(FIF_JPEG, tmpBitmap,
			path.c_str(), 90 | JPEG_OPTIMIZE | JPEG_PROGRESSIVE);

	if (saveResult == FALSE) {
		printf("[%s] could not save %s\n", __FUNCTION__, path.c_str());
		result = FALSE;
	}

	FreeImage_Unload(scaledNonLetterBoxedBitmap);
	FreeImage_Unload(tmpBitmap);

	return result;
}

bool GenerateImageTask::generateImageFiles_imageSrc(imageBf_t images) {
	bool      result = TRUE;
	bool firstIteration;
		uint32_t  i;
		uint32_t  j;
		uint32_t  size;
		uint32_t  quality;
		uint32_t  imgWidthInfo = 0;
		uint32_t  imgHeightInfo = 0;
		FIBITMAP* bitmap = NULL;
		FIBITMAP* thumbNailBitmap;
		FIBITMAP* rgbaBitmap;

		/* FreeImage's PNG reader does not always read very large images correctly.
		 * Use custom loader. */
		if (file->type == File::FILE_TYPE_PNG) {
			generateImageFiles_imageSrc_loadPngFile(file, &bitmap);
		} else {
			generateImageFiles_imageSrc_loadImageFile(file, &bitmap);

			if (bitmap == NULL) {
				printf("[%s] could not load source file %s\n",
		               __FUNCTION__, file->getPath().c_str());
				return FALSE;
			}

			//generateImageFiles_imageSrc_readMetaData(file, bitmap);
			generateImageFiles_imageSrc_autoRotate(file, &bitmap); // dependent on available metadata
		}

		size = IMAGE_SIZE_LARGEST; /* Go backwards due to resizing operation */
		firstIteration = TRUE;
		for(i=0; i < getNrBitsInBf(IMAGE_SIZE_ALL); i++ , size >>= 1) {
			uint32_t width           = (uint32_t) FreeImage_GetWidth(bitmap);
			uint32_t height          = (uint32_t) FreeImage_GetHeight(bitmap);
			uint32_t thumbNailWidth  = getImageWidth((ImageSize)size);
			uint32_t thumbNailHeight = getImageHeight((ImageSize)size);
			float    aspectRatio     = (float)width / (float)height;

			printf(":::::::::::::::::::::::::::: %u %u\n", width, thumbNailWidth);

			/* Save original image size */
			if (firstIteration) {
				imgWidthInfo = width;
				imgHeightInfo = height;
			}

			/* image size has not been requested, nothing to do */
			if ((size & images.size) == 0) {
				continue;
			}

			/* don't generate upscaled thumbnails. Allow minor difference */
			//if (thumbNailWidth > 1024 && thumbNailWidth - 256  > width) {
			//	continue;
			//}

			/* image source too small for current size */

			/* Regular 4:3 aspect ratio, thumb nail can be created directly */
			if (floatIsAlmostEqualTo(aspectRatio, ASPECT_RATIO_16_9)) {
				thumbNailBitmap = FreeImage_Rescale(bitmap,
						                            thumbNailWidth,
						                            thumbNailHeight,
						                            FILTER_LANCZOS3);
				if (thumbNailBitmap == NULL) {
					printf("[%s] unable to rescale %s\n", __FUNCTION__, file->getPath().c_str());
					result = FALSE;
					break;
				}
			} else { /* Another Aspect Ratio, create letter boxed thumb nail */

				thumbNailBitmap = generateImageFiles_imageSrc_createLetterBoxedThumbNail(bitmap, width, height, thumbNailWidth,
						                                     thumbNailHeight, aspectRatio);
				if (!thumbNailBitmap) {
					printf("[%s] unable to create letter boxed thumbnail %s\n", __FUNCTION__, file->getPath().c_str());
					result = FALSE;
					break;
				}

				file->letterboxed = TRUE;

				/* Save non letterboxed (but horizontally scaled) version    *
				 * of the file. This is useful for user downloads.      	 */
				if (firstIteration && !file->origAnimation) {
					result = generateImageFiles_imageSrc_createNonLetterBoxedImage(file, bitmap, width, aspectRatio, targetDirName);
					firstIteration = FALSE;
				}
			}

			quality = 0x01;
			for(j=0; j < getNrBitsInBf(THUMBNAIL_QUALITY_ALL); j++ , quality <<= 1) {
				bool saveResult;

				/* quality setting not requested */
				if ((quality & images.quality) == 0) {
					continue;
				}

				if (size > IMAGE_SIZE_THUMBNAIL_ALL && quality == THUMBNAIL_QUALITY_LOW) {
					continue;
				}

				printf("size %x sizeBf %x quality %x qualityBf %x\n",
						size, sizeBf, quality, qualityBf);

				if ((size & sizeBf) && (quality & qualityBf)) {
					string path = targetDirName + "/";

					path += getImageFileName(file, (ImageSize)size, (ThumbNailQuality)quality);

				    if (quality != THUMBNAIL_QUALITY_HIGH) {
				    	rgbaBitmap = FreeImage_ConvertTo32Bits(thumbNailBitmap);
				    	writeOptimizedPng(rgbaBitmap, (ThumbNailQuality)quality, path);
				    	FreeImage_Unload(rgbaBitmap);
				    } else {
	  			        saveResult = FreeImage_Save(getFreeImageOutputFormat((ThumbNailQuality)quality),
				    		                        thumbNailBitmap,
				    		                        path.c_str(),
				    		                        getFreeImageSaveFlags((ThumbNailQuality) quality,
													                      (ImageSize)size));
				        if (saveResult == FALSE) {
				    	    printf("[%s] could not save %s\n", __FUNCTION__, path.c_str());
				    	    result = FALSE;
				        }
				    }

				    printf("%s\n", path.c_str());
				}
			}

			FreeImage_Unload(thumbNailBitmap);
		}

		FreeImage_Unload(bitmap);

		// Move original file
		FileSystem::moveFile(file->getPath(), targetDirName + "/" + file->getHashString());

		if (file->origAnimation) {
			file->addMetaData("Animation", "");
			file->addMetaData("Animation.Width", to_string(imgWidthInfo));
			file->addMetaData("Animation.Height", to_string(imgHeightInfo));
		}
		else {
			file->addMetaData("Image", "");
			file->addMetaData("Image.Width", to_string(imgWidthInfo));
			file->addMetaData("Image.Height", to_string(imgHeightInfo));
		}

		file->writeInfoFile(targetDirName + "/" + file->getHashString() + ".info");

		if (result) {
			file->completelyProcessedImages = TRUE;
			file->completelyProcessedImagesTs = (uint32_t)time(NULL);
		}

	    return result;
}

string GenerateImageTask::getTimestampFromDuration(float duration) {
	char timestamp[100];

	uint32_t nrOfSeconds = (uint32_t)floor(duration);

	uint32_t hours;
	uint32_t minutes;
	uint32_t seconds;
	uint32_t mSeconds;

	hours   =  nrOfSeconds / 3600;
	minutes = (nrOfSeconds - 3600*hours) / 60;
	seconds = nrOfSeconds % 60;
	mSeconds = (uint32_t)((duration - (float)nrOfSeconds) * 1000);

	/* "HH:MM:SS.msec" */
	snprintf(timestamp, sizeof(timestamp), "%02u:%02u:%02u.%03u",
			 hours,
		     minutes,
		     seconds,
		     mSeconds);

	return string(timestamp);
}

bool GenerateImageTask::generateImageFiles_videoSrc(imageBf_t images) {
    bool result = TRUE;
    Video* video;
    Image* image;
    Image* thumbNailImage;
    string timestamp;
    bool firstIteration;
    uint32_t  i;
    uint32_t  j;
    uint32_t  size;
    uint32_t  quality;
    float aspectRatio;

    printf("%s ----> enter\n", __FUNCTION__);

    video = new Video(file);

    video->readMetaData();

    /* Don't take snapshot during fadeIn or before any representing image is seen. Select one in the middle */
    
    uint32_t duration = video->duration;
    if (duration < 10) {
    	timestamp = getTimestampFromDuration(duration / 2);
    } else {
    	// Try to catch title screen after possible fadein phase
    	timestamp = getTimestampFromDuration(10);
    }

    image = video->getImage(timestamp);
    if (image == NULL) {
    	printf("video->getImage failed with timestamp %s\n", timestamp.c_str());
    	delete video;
    	return FALSE;
    }

    aspectRatio = (float)image->width / (float)image->height;

    printf("w=%d h=%d aspectRatio=%f\n", image->width , image->height, aspectRatio);

    /* Crop image if it has wide format since a movie reel will be added. Instead
     * of letter boxing the image, the movie reel will have something else than a black background
     * so we remove the left and right side around the 4:3 image area. */
    if (!floatIsAlmostEqualTo(aspectRatio, ASPECT_RATIO_4_3) &&
    	!floatIsAlmostEqualTo(aspectRatio, ASPECT_RATIO_5_4)) { /* very likely case */
    	Image* croppedImage;
    	float newWidth = ASPECT_RATIO_16_9 * (float)image->height;
		printf("newWidth-------------------> %f\n", newWidth);
    	float offset = ((float)image->width - newWidth) / 2;
    	uint32_t left = (uint32_t)offset;
    	uint32_t top  = 0;
    	uint32_t right = image->width - (uint32_t)offset;
    	uint32_t bottom = image->height - 1;

    	croppedImage = image->createCroppedImage(left, top, right, bottom);
    	assert(croppedImage != NULL);
    	delete image;
    	image = croppedImage;
    }

	//size = IMAGE_SIZE_HIGH_RES_2K;//; /* Go backwards due to resizing operation */
	//firstIteration = TRUE;
	//for(i=0; i < getNrBitsInBf(IMAGE_SIZE_HIGH_RES_ALL); i++ , size >>= 1) {
		//uint32_t thumbNailWidth  = 1920;//getImageWidth((ImageSize)size);
		//uint32_t thumbNailHeight = 1080;//getImageHeight((ImageSize)size);
        //Image* filmReelThumbNail;

		/* image size has not been requested, nothing to do */
		//if ((size & images.size) == 0) {
		//	continue;
	//	}

		thumbNailImage = image->createRescaledImage(1920, 1080);
		assert(thumbNailImage != NULL);

		//filmReelThumbNail = thumbNailImage->createFilmReelImage();
		//assert(filmReelThumbNail != NULL);

		//quality = 0x01;
		//for(j=0; j < getNrBitsInBf(THUMBNAIL_QUALITY_ALL); j++ , quality <<= 1) {

			/* quality setting not requested */
			//if ((quality & images.quality) == 0) {
			//	continue;
		//	}

			//i//f ((size & sizeBf) && (quality & qualityBf)) {
				string path = targetDirName + "/";

				path += getImageFileName(file, IMAGE_SIZE_HIGH_RES_2K, THUMBNAIL_QUALITY_HIGH);

				//if (quality != THUMBNAIL_QUALITY_HIGH) {
				//	filmReelThumbNail->writeIndexedPng(path);
				//} else {
					thumbNailImage->writeImage(path);
				//}

				printf("%s\n", path.c_str());
			//}

	//} /* end of quality loop */

		delete thumbNailImage;
		//delete filmReelThumbNail;
	//} /* end of size loop */

	if (result) {
		file->completelyProcessedImages = TRUE;
		file->completelyProcessedImagesTs = (uint32_t)time(NULL);
		writeHashFile();
	}

	delete image;
    delete video;
	return result;
}

bool GenerateImageTask::generateImageFiles(imageBf_t images) {
	if (file->typeIsImage()) {
		return generateImageFiles_imageSrc(images);
	} else {
		return generateImageFiles_videoSrc(images);
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t GenerateImageTask::getImageWidth(ImageSize size) {
	switch (size) {
	    case IMAGE_SIZE_THUMBNAIL_SMALL:
			return 160;
	    case IMAGE_SIZE_THUMBNAIL_MEDIUM:
			return 320;
	    case IMAGE_SIZE_THUMBNAIL_LARGE:
			return 640;
	    case IMAGE_SIZE_HIGH_RES_1K:
	    	return 1024;
	    case IMAGE_SIZE_HIGH_RES_2K:
			return 1920;
	    case IMAGE_SIZE_HIGH_RES_4K:
	    	return 4096;
	    case IMAGE_SIZE_HIGH_RES_8K:
	    	return 8192;
	    case IMAGE_SIZE_UNKNOWN:
	    case IMAGE_SIZE_THUMBNAIL_ALL:
	    case IMAGE_SIZE_HIGH_RES_ALL:
	    default:
	        return 0;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t GenerateImageTask::getImageHeight(ImageSize size) {
	switch (size) {
	    case IMAGE_SIZE_THUMBNAIL_SMALL:
			return 120;
	    case IMAGE_SIZE_THUMBNAIL_MEDIUM:
			return 240;
	    case IMAGE_SIZE_THUMBNAIL_LARGE:
			return 480;
	    case IMAGE_SIZE_HIGH_RES_1K:
	    	return 768;
	    case IMAGE_SIZE_HIGH_RES_2K:
			return 1080;
	    case IMAGE_SIZE_HIGH_RES_4K:
	    	return 3072;
	    case IMAGE_SIZE_HIGH_RES_8K:
	    	return 6144;
	    case IMAGE_SIZE_UNKNOWN:
	    case IMAGE_SIZE_THUMBNAIL_ALL:
	    case IMAGE_SIZE_HIGH_RES_ALL:
	    default:
	        return 0;
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

string GenerateImageTask::getImageSizeSuffix(ImageSize size) {
	string sizeSuffix    = "";

	switch (size) {
	case IMAGE_SIZE_THUMBNAIL_SMALL:
		sizeSuffix = "_0160";
		break;
	case IMAGE_SIZE_THUMBNAIL_MEDIUM:
		sizeSuffix = "_0320";
		break;
	case IMAGE_SIZE_THUMBNAIL_LARGE:
		sizeSuffix = "_0640";
		break;
	case IMAGE_SIZE_HIGH_RES_1K:
		sizeSuffix = "_1024";
		break;
	case IMAGE_SIZE_HIGH_RES_2K:
		sizeSuffix = "_1080";
		break;
	case IMAGE_SIZE_HIGH_RES_4K:
		sizeSuffix = "_4096";
		break;
	case IMAGE_SIZE_HIGH_RES_8K:
		sizeSuffix = "_8192";
		break;
    case IMAGE_SIZE_UNKNOWN:
    case IMAGE_SIZE_THUMBNAIL_ALL:
    case IMAGE_SIZE_HIGH_RES_ALL:
    default:
		sizeSuffix = "";
		break;
	}

	return sizeSuffix;
}

/*-------------------------------------------------------------------------------------------------------------------*/

string GenerateImageTask::getThumbNailQualitySuffix(ThumbNailQuality quality) {
	string qualitySuffix = "";

	switch(quality) {
		    case THUMBNAIL_QUALITY_LOW:
		    	qualitySuffix = "_q1";

			    break;
		    case THUMBNAIL_QUALITY_HIGH:
			    qualitySuffix = "_q2";

		        break;
		    case THUMBNAIL_QUALITY_UNKNOWN:
		    case THUMBNAIL_QUALITY_ALL:
		    default:
		    	qualitySuffix = "";
			    break;
		}

	return qualitySuffix;
}

/*-------------------------------------------------------------------------------------------------------------------*/

string GenerateImageTask::getThumbNailFileType(ThumbNailQuality quality, bool isVideo) {
	string fileType;

	switch(quality) {
		    case THUMBNAIL_QUALITY_LOW:
		    	fileType = ".png";
			    break;
		    case THUMBNAIL_QUALITY_HIGH:
		    	fileType = ".jpg";
		        break;
		    case THUMBNAIL_QUALITY_UNKNOWN:
		    case THUMBNAIL_QUALITY_ALL:
		    default:
		    	fileType = "";
			    break;
		}

	if (isVideo) {
//		fileType = ".gif";
	}

	return fileType;
}

/*-------------------------------------------------------------------------------------------------------------------*/

string GenerateImageTask::getImageFileName(File* file, ImageSize size, ThumbNailQuality quality) {
	string fileName      = "";
	string sizeSuffix    = "";
	string qualitySuffix = "";
	string fileType      = "";
	string suffix        = "";

	fileName = file->getHashString();

    sizeSuffix    = getImageSizeSuffix(size);
	qualitySuffix = getThumbNailQualitySuffix(quality);
	fileType      = getThumbNailFileType(quality, file->typeIsVideo());

	if (size > IMAGE_SIZE_THUMBNAIL_ALL) {
		qualitySuffix = "";
	}

	suffix = sizeSuffix + qualitySuffix + fileType;
	fileName += suffix;
	file->appendOutputSuffixes(suffix);

	return fileName;
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t  GenerateImageTask::getNrQuantizationColors(ThumbNailQuality quality) {
    uint32_t nrOfColors = 256;

    switch(quality) {
	    case THUMBNAIL_QUALITY_LOW:
	    	nrOfColors = 8;
		    break;
	    case THUMBNAIL_QUALITY_HIGH:
	    case THUMBNAIL_QUALITY_UNKNOWN:
	    case THUMBNAIL_QUALITY_ALL:
	    default:
		    break;
	}

	return nrOfColors;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool GenerateImageTask::floatIsAlmostEqualTo(float value, float ref) {
	float diff = abs(value-ref);
	float fmin = 0.00001F; /* Smallest float value for this program */

	/* This comparison will return false for larger difference, or for *
	 * extreme values such as inf, -inf and NaN. 	                   */
	return (diff < fmin);
}
