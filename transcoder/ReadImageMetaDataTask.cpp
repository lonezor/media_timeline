#include "ReadImageMetaDataTask.h"
#include "FileSystem.h"
#include "PngReader.h"
#include "PngWriter.h"
#include "general.h"

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

ReadImageMetaDataTask::ReadImageMetaDataTask(File* file) {
	this->file          = file;
}

/*-------------------------------------------------------------------------------------------------------------------*/

void ReadImageMetaDataTask::execute() {
	if (file->typeIsImage()) {
		FIBITMAP* bitmap = NULL;

		/* FreeImage's PNG reader does not always read very large images correctly.
		 * Use custom loader. */
		if (file->type != File::FILE_TYPE_PNG) {
			loadImageFile(file, &bitmap);

			if (bitmap == NULL) {
				printf("[%s] could not load source file %s\n",
						__FUNCTION__, file->getPath().c_str());
				return;
			}

			readMetaData(file, bitmap);
			FreeImage_Unload(bitmap);
		}
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void ReadImageMetaDataTask::readImageMetaData(FIBITMAP* bitmap, FREE_IMAGE_MDMODEL model, File* file) {
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

			file->addMetaData("Exif." + string(tagKey), string(tagValue));
		} while (FreeImage_FindNextMetadata(metaData,&tag));
	}
}

/*-------------------------------------------------------------------------------------------------------------------*/

void ReadImageMetaDataTask::loadImageFile(File* file, FIBITMAP** bitmap) {
	*bitmap = FreeImage_Load(getFreeImageFormat(file->type),
			file->getPath().c_str(),
			getFreeImageLoadFlags(file->type));
}

/*-------------------------------------------------------------------------------------------------------------------*/

uint32_t ReadImageMetaDataTask::getFreeImageLoadFlags(File::FileType fileType) {
	uint32_t flags = 0;

	switch (fileType) {
	    case File::FILE_TYPE_JPG:
	    	flags = JPEG_FAST;
	    	break;
	    default:
	    	flags = 0;
	    	break;
	}

	return flags;
}

/*-------------------------------------------------------------------------------------------------------------------*/

FREE_IMAGE_FORMAT ReadImageMetaDataTask::getFreeImageFormat(File::FileType fileType) {
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

void ReadImageMetaDataTask::readMetaData(File* file, FIBITMAP* bitmap) {
	readImageMetaData(bitmap, FIMD_EXIF_MAIN, file);
	readImageMetaData(bitmap, FIMD_EXIF_EXIF, file);
	readImageMetaData(bitmap, FIMD_EXIF_GPS, file);
	readImageMetaData(bitmap, FIMD_EXIF_MAKERNOTE, file);
	readImageMetaData(bitmap, FIMD_EXIF_INTEROP, file);
}

/*-------------------------------------------------------------------------------------------------------------------*/
