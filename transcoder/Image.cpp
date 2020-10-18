#include "Image.h"
#include "PngWriter.h"
#include "general.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "libimagequant.h"

Image::Image(File* srcFile) {
	srcFile->determineFileType();

	this->bitmap = NULL;
	this->fileType = srcFile->type;
	this->srcFilePath = srcFile->getPath();
	this->width = 0;
	this->height = 0;
	this->quantizationPerformed = FALSE;
	this->srcImagLoaded = FALSE;
}

Image::Image() {
	this->bitmap = NULL;
	this->fileType = File::FILE_TYPE_UNKNOWN;
	this->srcFilePath = "";
	this->width = 0;
	this->height = 0;
	this->quantizationPerformed = FALSE;
	this->srcImagLoaded = FALSE;
}

Image::~Image() {
	if (this->bitmap != NULL) {
		FreeImage_Unload(this->bitmap);
	}
}

FREE_IMAGE_FORMAT Image::getFreeImageFormat(File::FileType fileType) {
	switch (fileType) {
	    case File::FILE_TYPE_JPG:
	    	return FIF_JPEG;
	    case File::FILE_TYPE_PNG:
	    	return FIF_PNG;
	    case File::FILE_TYPE_UNKNOWN:
	    default:
		    return FIF_UNKNOWN;
	}
}

uint32_t Image::getFreeImageLoadFlags(File::FileType fileType) {
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

Image* Image::createRescaledImage(uint32_t targetWidth, uint32_t targetHeight) {
	FIBITMAP* rescaledImage;
	Image* image;

	rescaledImage = FreeImage_Rescale(bitmap, targetWidth, targetHeight, FILTER_LANCZOS3);

	if (!rescaledImage) return NULL;

	image = new Image();
	image->bitmap = rescaledImage;
	image->width  = FreeImage_GetWidth(rescaledImage);
	image->height = FreeImage_GetHeight(rescaledImage);

	return image;
}

Image* Image::createCroppedImage(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) {
	FIBITMAP* croppedImage;
	Image* image;

	croppedImage = FreeImage_Copy(bitmap, left, top, right, bottom);

	if (!croppedImage) return NULL;

	image = new Image();
	image->bitmap = croppedImage;
	image->width  = FreeImage_GetWidth(croppedImage);
	image->height = FreeImage_GetHeight(croppedImage);

	return image;
}

void Image::drawRectangle(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom,
		                  uint8_t red, uint8_t green, uint8_t blue) {
	uint32_t xStart = left;
	uint32_t xStop = right;
	uint32_t yStart = top;
	uint32_t yStop = bottom;
	RGBQUAD  color;
	uint32_t x,y;

	//printf("[%s] left=%u top=%u right=%u bottom=%u r=%u g=%u b=%u\n", __FUNCTION__,
	//		left, top, right, bottom, red, green, blue);

	color.rgbRed      = red;
	color.rgbGreen    = green;
	color.rgbBlue     = blue;
	color.rgbReserved = 0;

	for(y=yStart; y <= yStop; y++) { // top
		for(x=xStart; x <= xStop; x++) {
			FreeImage_SetPixelColor(bitmap, x, y, &color);
		}
	}
}

Image* Image::createFilmReelImage(void) {
	FIBITMAP* filmReelImage;
	Image* image;
	float reelProp     = 0.25; /* how much of source image (both top and bottom) */
	uint32_t reelHeightPx = (uint32_t)(((float)this->height * reelProp) / 2);
	uint32_t reelPunctureWidthPx;
	uint32_t reelPunctureHeightPx;
	uint32_t spaceWidthPx;
	uint32_t spaceHeightPx;
	uint32_t nrOfPunctures;
	uint32_t xOffset;
	uint32_t i;
	RGBQUAD   color;

	filmReelImage = FreeImage_Copy(bitmap, 0, 0, this->width-1, this->height-1);

	if (!filmReelImage) return NULL;

	image = new Image();
	image->bitmap = filmReelImage;
	image->width  = FreeImage_GetWidth(filmReelImage);
	image->height = FreeImage_GetHeight(filmReelImage);

#if 0

	/* Draw black background */
	image->drawRectangle(0, 0,this->width-1,reelHeightPx, 0, 0, 0);
	image->drawRectangle(0, this->height-1-reelHeightPx,this->width-1, this->height-1, 0, 0, 0);


	/* Draw rectangular puncture holes with white background */
	reelPunctureHeightPx = (uint32_t)((float)reelHeightPx * 0.40);
	reelPunctureWidthPx = reelPunctureHeightPx; /* square */
	spaceWidthPx = (reelHeightPx - reelPunctureHeightPx) / 2;
	spaceHeightPx = spaceWidthPx;
	spaceWidthPx *= 2;

	nrOfPunctures = this->width / (reelPunctureWidthPx + spaceWidthPx);

	xOffset = spaceWidthPx;
	for (i=0; i<nrOfPunctures; i++) {

		image->drawRectangle(xOffset, this->height-1-reelHeightPx+spaceHeightPx,
				             xOffset + reelPunctureWidthPx, this->height-1-spaceHeightPx,
							 255, 255, 255);

		image->drawRectangle(xOffset, reelHeightPx - spaceHeightPx - reelPunctureHeightPx,
				             xOffset + reelPunctureWidthPx, reelHeightPx - spaceHeightPx,
							 255, 255, 255);

		xOffset += reelPunctureWidthPx + spaceWidthPx;
	}

	#endif

	return image;
}


bool Image::loadImage() {
	bool result = TRUE;

	bitmap = FreeImage_Load(getFreeImageFormat(fileType), srcFilePath.c_str(), getFreeImageLoadFlags(fileType));
	if (bitmap == NULL) {
		printf("unable to load %s into memory\n", srcFilePath.c_str());
		result = FALSE;
	}

    return result;
}

bool   Image::writeImage(string path) {
	printf("writing %s\n", path.c_str());
	 return FreeImage_Save(FIF_JPEG, bitmap, path.c_str(), 90 | JPEG_OPTIMIZE | JPEG_PROGRESSIVE);
}

bool   Image::writeIndexedPng(string path) {
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
	FIBITMAP*          rgbaBitmap = bitmap;
	bool               rgbaBitmapAlloc = FALSE;

	bitDepth = FreeImage_GetBPP(bitmap);

	/* Expect RGBA */
	if (bitDepth != 32) {
		rgbaBitmap = FreeImage_ConvertTo32Bits(bitmap);
		rgbaBitmapAlloc = TRUE;
	}

	width  = FreeImage_GetWidth(rgbaBitmap);
	height = FreeImage_GetHeight(rgbaBitmap);

	nrOfColors = 4;

	scanLines = (BYTE**) malloc(sizeof(BYTE*) * height);
	assert(scanLines);

	scanLinesIndexedColor = (uint8_t**) malloc(sizeof(uint8_t*) * height);
	assert(scanLinesIndexedColor);

	for(y=0; y < height; y++) {
		/* The scan lines are stored according to the screen     *
		 * coordinate system (rendering from top to bottom).     *
		 * So we read them backwards to be compatible with       *
		 * how the PNG quantization library operates on images.  */
		scanLine = FreeImage_GetScanLine(rgbaBitmap, height-1-y);
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

    liq_set_dithering_level(res, 0.8F);

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

	if (rgbaBitmapAlloc) {
		FreeImage_Unload(rgbaBitmap);
	}

    return TRUE;
}
