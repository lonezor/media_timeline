#include "PngWriter.h"
#include "general.h"
#include <assert.h>

/*-------------------------------------------------------------------------------------------------------------------*/

PngWriter::PngWriter(string path) {
	this->path      = path;
	this->file      = NULL;
	this->pngStruct = NULL;
	this->pngInfo   = NULL;
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool PngWriter::init() {
	file = fopen(path.c_str(),"wb");
	if (!file) {
		return FALSE;
	}

	pngStruct = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!pngStruct) {
    	printf("png_create_write_struct failed!\n");
    	fclose(file);
    	return FALSE;
    }

    pngInfo = png_create_info_struct(pngStruct);
    if (!pngInfo) {
    	printf("png_create_write_struct failed!\n");
    	fclose(file);
    	return FALSE;
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

    /* set highest compression as communication bandwidth is more limited than
     * client CPUs */
    png_set_compression_level(pngStruct, 9);

	return (file != NULL);
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool PngWriter::write(uint8_t** scanLinesIndexedColor, const liq_palette* palette, uint16_t width, uint16_t height) {
	bool       result;
	png_colorp pngPalette;
	uint32_t   i;
	int        bitDepth = 8;

	result = init();
    if (!result) {
    	return FALSE;
    }

    /* Write PNG header */
    png_set_filter(pngStruct, PNG_FILTER_TYPE_BASE, PNG_FILTER_VALUE_NONE);
    png_set_IHDR(pngStruct, pngInfo, width, height, bitDepth, PNG_COLOR_TYPE_PALETTE,
    		     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    pngPalette = (png_colorp) png_malloc(pngStruct, palette->count * sizeof(png_color));
    assert(pngPalette);

    printf("palette size: %d\n", palette->count);

    for(i=0; i < palette->count; i++) {
    	png_colorp p = (png_colorp)&pngPalette[i];
    	p->red   = palette->entries[i].r;
    	p->green = palette->entries[i].g;
    	p->blue  = palette->entries[i].b;

    	printf("palette index %d: r=%d g=%d b=%d\n", i, p->red, p->green, p->blue);
    }

    png_set_PLTE(pngStruct,pngInfo, pngPalette, palette->count);
    png_write_info(pngStruct, pngInfo);

    if (setjmp(png_jmpbuf(pngStruct))) {
    	printf("[%s] failed to setup libpng long jump error handler!\n", __FUNCTION__);
    	cleanup();
    	return FALSE;
    }

    /* Write data */
    png_write_image(pngStruct, scanLinesIndexedColor);

    if (setjmp(png_jmpbuf(pngStruct))) {
    	printf("[%s] failed to setup libpng long jump error handler!\n", __FUNCTION__);
    	cleanup();
    	return FALSE;
    }

    /* Finalize file */
    png_write_end(pngStruct, NULL);
    png_free(pngStruct, pngPalette);

    return cleanup();
}

/*-------------------------------------------------------------------------------------------------------------------*/

bool PngWriter::cleanup() {
    png_destroy_write_struct(&pngStruct, &pngInfo);
	fclose(file);

	return TRUE;
}

/*-------------------------------------------------------------------------------------------------------------------*/

