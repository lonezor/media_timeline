#include "PngReader.h"

#include <assert.h>

PngReader::PngReader(string path) {
	this->path = path;
}

FIBITMAP* PngReader::getBitmap() {
	FIBITMAP*    bitmap;
	FILE*        file;
	png_structp  pngStruct;
	png_infop    pngInfo;
	png_uint_32  width;
	png_uint_32  height;
	png_byte     colorType;
	png_byte     bitDepth;
	png_bytepp   rowPointers;
	png_uint_32  x;
	png_uint_32  y;
	uint32_t*    pixel;

	file = fopen(path.c_str(), "rb");
	if (!file) {
		return NULL;
	}

	pngStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!pngStruct) {
		printf("[%s] png_create_read_struct failed!\n", __FUNCTION__);
		fclose(file);
		return NULL;
	}

	pngInfo = png_create_info_struct(pngStruct);
	if (!pngInfo) {
		printf("[%s] png_create_info_struct failed!\n", __FUNCTION__);
		fclose(file);
		return NULL;
	}

    if (setjmp(png_jmpbuf(pngStruct))) {
    	printf("[%s] failed to setup libpng long jump error handler!\n", __FUNCTION__);
    	fclose(file);
    	return FALSE;
    }

    png_init_io(pngStruct, file);

    if (setjmp(png_jmpbuf(pngStruct))) {
    	printf("[%s] failed to setup libpng long jump error handler!\n", __FUNCTION__);
    	fclose(file);
    	return FALSE;
    }

    png_read_info(pngStruct, pngInfo);

    width     = png_get_image_width(pngStruct, pngInfo);
    height    = png_get_image_height(pngStruct, pngInfo);
    colorType = png_get_color_type(pngStruct, pngInfo);
    bitDepth  = png_get_bit_depth(pngStruct, pngInfo);

    /* Convert different image formats into regular 8bit RGBA format */
    if (bitDepth == 16) {
    	png_set_strip_16(pngStruct);
    }

    if (colorType == PNG_COLOR_TYPE_PALETTE) {
    	png_set_palette_to_rgb(pngStruct);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
    	png_set_expand_gray_1_2_4_to_8(pngStruct);
    }

    if (png_get_valid(pngStruct, pngInfo, PNG_INFO_tRNS)) {
    	png_set_tRNS_to_alpha(pngStruct);
    }

    if (colorType == PNG_COLOR_TYPE_RGB ||
        colorType == PNG_COLOR_TYPE_GRAY ||
		colorType == PNG_COLOR_TYPE_PALETTE) {
    	png_set_filler(pngStruct, 0xff, PNG_FILLER_AFTER);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    	png_set_gray_to_rgb(pngStruct);
    }

    png_read_update_info(pngStruct, pngInfo);

    rowPointers = (png_bytepp) malloc(sizeof(png_bytep)*height);
    assert(rowPointers);

    for(y=0; y < height; y++) {
    	rowPointers[y] = (png_bytep) malloc(png_get_rowbytes(pngStruct, pngInfo));
    }

    png_read_image(pngStruct, rowPointers);

    bitmap = FreeImage_Allocate(width, height, 24);
    assert(bitmap);

    for(y=0; y < height; y++) {
    	pixel = (uint32_t*) rowPointers[height-y-1];
    	for(x=0; x < width; x++) {
    		uint8_t* channel = (uint8_t*)pixel;
    		RGBQUAD rgbQuad;
    		rgbQuad.rgbRed   = channel[0];
    		rgbQuad.rgbGreen = channel[1];
    		rgbQuad.rgbBlue  = channel[2];
    		FreeImage_SetPixelColor(bitmap, x, y, &rgbQuad);
    		pixel++;
    	}
    }

    for(y=0; y < height; y++) {
    	free(rowPointers[y]);
    }
    free(rowPointers);

    png_destroy_read_struct(&pngStruct, &pngInfo, NULL);
    fclose(file);

    return bitmap;
}


