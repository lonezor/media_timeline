#ifndef PNGWRITER_H_
#define PNGWRITER_H_

/* The PngWriter is used to provide support for writing indexed color PNG files
 * which the generic FreeImage library does not support. The image input is
 * the image quantizizer's raw data.
 */

#include <string>
#include <png.h>
#include "libimagequant.h"

using namespace std;

class PngWriter {
public:

	PngWriter(string path);

	bool write(uint8_t** scanLinesIndexedColor, const liq_palette* palette, uint16_t width, uint16_t height);

private:
	string      path;
	FILE*       file;
	png_structp pngStruct;
	png_infop   pngInfo;

	bool init(void);
	bool cleanup(void);
};




#endif /* PNGWRITER_H_ */
