#ifndef _CAIRO_PNG_WRITER_H_
#define _CAIRO_PNG_WRITER_H_

#include <cairo.h>

#define PNG_COMPRESSION_LEVEL_NONE (0)
#define PNG_COMPRESSION_LEVEL_MIN (1)
#define PNG_COMPRESSION_LEVEL_GOOD (6)
#define PNG_COMPRESSION_LEVEL_MAX (9)

#define DEFAULT_PNG_COMPRESSION_LEVEL (PNG_COMPRESSION_LEVEL_MIN)

//------------------------------------------------------------------------------------------------------------------------

// Essentially an enhanced version of cairo_surface_write_to_png() with compression level support
// Webtest related image data gives 45% performance improvement with PNG_COMPRESSION_LEVEL_MIN compared to
// cairo_surface_write_to_png()
void
cairoPngWriter_writeToFileSystem(cairo_surface_t* surface, const char* pngFilePath, int compressionLevel); // 0-9 range

//------------------------------------------------------------------------------------------------------------------------

#endif /* _CAIRO_PNG_WRITER_H_ */
