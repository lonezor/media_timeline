
#include "cairo_png_writer.hpp"

#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define COMPONENT LOG_COMPONENT_OFFLOADSERVER_SNAPSHOTANALYZER_CAIRO_PNG_WRITER

//-----------------------------------------------------------------------------------------------------------

static void
cairoPngWriter_writeToFs(uint8_t* pixelBuffer,
                         int width,
                         int height,
                         cairo_format_t format,
                         int stride,
                         int compressionLevel,
                         const char* pngFilePath)
{
    FILE* pngFile;

    pngFile = fopen(pngFilePath, "wb");
    assert(pngFile);

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png);

    png_infop info = png_create_info_struct(png);
    assert(info);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(pngFile);
        return;
    }

    // To get max performance we keep alpha channel
    // and let the PNG library deal with Cairo's
    // internal BGRA representation.
    // NOTE: even CAIRO_FORMAT_RGB24 use 4 channels
    int depth = 8;
    png_set_IHDR(png,
                 info,
                 width,
                 height,
                 depth,
                 PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_set_compression_level(png, compressionLevel);

    png_byte** rows = NULL;
    rows = (png_byte**)png_malloc(png, height * sizeof(png_byte*));
    assert(rows);

    int y;
    uint8_t* pxBufferRow = pixelBuffer;
    for (y = 0; y < height; y++) {
        rows[y] = pxBufferRow;
        pxBufferRow += stride;
    }

    png_init_io(png, pngFile);
    png_set_rows(png, info, rows);
    png_write_png(png, info, PNG_TRANSFORM_BGR, NULL);
    png_free(png, rows);

}

//-----------------------------------------------------------------------------------------------------------

void
cairoPngWriter_writeToFileSystem(cairo_surface_t* surface, const char* pngFilePath, int compressionLevel)
{
    assert(surface);
    assert(pngFilePath);

    uint8_t* cairoBuffer = cairo_image_surface_get_data(surface);
    assert(cairoBuffer);

    int width = cairo_image_surface_get_width(surface);
    int height = cairo_image_surface_get_height(surface);
    int stride = cairo_image_surface_get_stride(surface);
    cairo_format_t format = cairo_image_surface_get_format(surface);

    cairoPngWriter_writeToFs(cairoBuffer, width, height, format, stride, compressionLevel, pngFilePath);
}

//-----------------------------------------------------------------------------------------------------------
