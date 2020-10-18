#ifndef _IMAGE_H_
#define _IMAGE_H_

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct _image_t image_t;

//-----------------------------------------------------------------------------------------------------------

image_t*
image_create(void);

void
image_destroy(image_t* img);

bool
image_loadFromFile(image_t* img, const char* path, int width, int height);

void
image_rescale(image_t* img, int width, int height);

void
image_saveAsPng(image_t* img, char* path);

GtkWidget*
image_new_GtkImage(image_t* img);

GdkPixbuf*
image_new_GdkPixBuf(image_t* img);

GdkPixbuf*
image_get_gtkPixBuf(image_t* img);

int
image_get_width(image_t* img);

int
image_get_height(image_t* img);

void
image_set_width(image_t* img, int width);

void
image_set_height(image_t* img, int height);

int
image_get_scaledHeight(image_t* img, int newWidth);

void*
image_get_pixelAddress(image_t* img, int x, int y);

int
image_get_pixelSize(image_t* img);

void
image_get_pixel(image_t* img, uint8_t* p, uint8_t* red, uint8_t* green, uint8_t* blue);

void
image_set_pixel(image_t* img, uint8_t* p, uint8_t red, uint8_t green, uint8_t blue);

int
image_compareScanline(image_t* img1, image_t* img2, int y1, int y2);

void
image_verticalShift(image_t* img, int y, int nrRows, int shift);

void
image_setAllPixels(image_t* img, uint8_t channelValue);

//-----------------------------------------------------------------------------------------------------------

#endif /* _IMAGE_H_ */
