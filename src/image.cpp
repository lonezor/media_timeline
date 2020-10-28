#include "image.hpp"


#include <assert.h>
#include <stdio.h>
#include <string.h>

#define COMPONENT LOG_COMPONENT_OFFLOADSERVER_SNAPSHOTANALYZER_IMAGE

//------------------------------------------------------------------------------------------------------------------------

struct _image_t
{
    GdkPixbuf* pixbuf;
    int width;
    int height;
    int nrChannels;
    int bitsPerSample;
    int colorSpace;
};

//------------------------------------------------------------------------------------------------------------------------

image_t*
image_create()
{
    image_t* img = (image_t*)calloc(1, sizeof(image_t));
    assert(img);

    img->pixbuf = NULL;
    img->width = 0;
    img->height = 0;
    img->nrChannels = 0;

    return img;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_destroy(image_t* img)
{
    assert(img);
    free(img);
}

//------------------------------------------------------------------------------------------------------------------------

bool
image_loadFromFile(image_t* img, const char* path, int width, int height)
{
    GError* err = NULL;
    bool success = TRUE;

    img->pixbuf= 
        gdk_pixbuf_new_from_file_at_scale(path,
                                   width,
                                   height,
                                   TRUE,
                                   &err);
    if (!img->pixbuf) {
        printf("gdk_pixbuf_new_from_file fail: %s\n", path);
        return FALSE;
    }

    img->width = gdk_pixbuf_get_width(img->pixbuf);
    img->height = gdk_pixbuf_get_height(img->pixbuf);
    img->nrChannels = gdk_pixbuf_get_n_channels(img->pixbuf);
    img->colorSpace = gdk_pixbuf_get_colorspace(img->pixbuf);
    img->bitsPerSample = gdk_pixbuf_get_bits_per_sample(img->pixbuf);

    success &= (img->colorSpace == GDK_COLORSPACE_RGB);
    success &= (img->bitsPerSample == 8);
    success &= (img->nrChannels == 3);

    // used as buffer for raw pixel access, ensure it is allocated even though GTK does not reference it.
    g_object_ref(img->pixbuf);

    return success;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_rescale(image_t* img, int width, int height)
{
    GdkPixbuf* pxBuf;

    pxBuf = gdk_pixbuf_scale_simple(img->pixbuf, width, height, GDK_INTERP_HYPER);

    g_clear_object(&img->pixbuf);
    img->pixbuf = pxBuf;
    img->width = gdk_pixbuf_get_width(img->pixbuf);
    img->height = gdk_pixbuf_get_height(img->pixbuf);
}

//------------------------------------------------------------------------------------------------------------------------

int
image_get_width(image_t* img)
{
    assert(img);
    return img->width;
}

//------------------------------------------------------------------------------------------------------------------------

int
image_get_height(image_t* img)
{
    assert(img);
    return img->height;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_set_width(image_t* img, int width)
{
    assert(img);

    // Only allow setting width when there is no pixel buffer
    if (!img->pixbuf)
        img->width = width;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_set_height(image_t* img, int height)
{
    assert(img);

    // Only allow setting width when there is no pixel buffer
    if (!img->pixbuf)
        img->height = height;
}

//------------------------------------------------------------------------------------------------------------------------

int
image_get_scaledHeight(image_t* img, int newWidth)
{
    float width = (float)img->width;
    float height = (float)img->height;
    float aspectRatio = width / height;
    int scaledHeight = (int)(((float)newWidth) / aspectRatio);

    // FFmpeg requires an height even divisible by two
    if (scaledHeight % 2 != 0) {
        scaledHeight++;
    }

    return scaledHeight;
}

//------------------------------------------------------------------------------------------------------------------------

void*
image_get_pixelAddress(image_t* img, int x, int y)
{
    int rowstride;
    guchar *pixels, *p;

    assert(x >= 0 && x < img->width);
    assert(y >= 0 && y < img->height);

    rowstride = gdk_pixbuf_get_rowstride(img->pixbuf);
    pixels = gdk_pixbuf_get_pixels(img->pixbuf);

    p = pixels + y * rowstride + x * img->nrChannels;

    return p;
}

//------------------------------------------------------------------------------------------------------------------------

int
image_get_pixelSize(image_t* img)
{
    int bytesPerSample = gdk_pixbuf_get_bits_per_sample(img->pixbuf) / 8;

    return img->nrChannels * bytesPerSample;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_get_pixel(image_t* img, uint8_t* p, uint8_t* red, uint8_t* green, uint8_t* blue)
{
    (void)img;
    *red = p[0];
    *green = p[1];
    *blue = p[2];
}

//------------------------------------------------------------------------------------------------------------------------

void
image_set_pixel(image_t* img, uint8_t* p, uint8_t red, uint8_t green, uint8_t blue)
{
    (void)img;
    p[0] = red;
    p[1] = green;
    p[2] = blue;
}

//------------------------------------------------------------------------------------------------------------------------

GtkWidget*
image_new_gtkImage(image_t* img)
{
    return gtk_image_new_from_pixbuf(img->pixbuf);
}

//------------------------------------------------------------------------------------------------------------------------

GdkPixbuf*
image_new_gdkPixBuf(image_t* img)
{
    return gdk_pixbuf_copy(img->pixbuf);
}

//------------------------------------------------------------------------------------------------------------------------

GdkPixbuf*
image_get_gtkPixBuf(image_t* img)
{
    return img->pixbuf;
}

//------------------------------------------------------------------------------------------------------------------------

void
image_saveAsPng(image_t* img, char* path)
{
    GError* err = NULL;

    gdk_pixbuf_save(img->pixbuf, path, "png", &err, "compression", "6", NULL);

    if (err != NULL) {
        printf("gdk_pixbuf_save failed: %s\n", __FUNCTION__);
    }
}

//------------------------------------------------------------------------------------------------------------------------

int
image_compareScanline(image_t* img1, image_t* img2, int y1, int y2)
{
    assert(img1);
    assert(img2);
    assert(img1->width == img2->width);
    assert(img1->height == img2->height);
    assert(img1->nrChannels == img2->nrChannels);
    assert(y1 >= 0 && y1 < img1->height);
    assert(y2 >= 0 && y2 < img2->height);

    int rowstride = gdk_pixbuf_get_rowstride(img1->pixbuf);

    guchar* pixels = NULL;
    guchar* p1 = NULL;
    guchar* p2 = NULL;

    pixels = gdk_pixbuf_get_pixels(img1->pixbuf);
    p1 = pixels + (y1 * rowstride);

    pixels = gdk_pixbuf_get_pixels(img2->pixbuf);
    p2 = pixels + (y2 * rowstride);

    return memcmp(p1, p2, rowstride);
}

//------------------------------------------------------------------------------------------------------------------------

void
image_verticalShift(image_t* img, int y, int nrRows, int shift)
{
    assert(img);
    assert(y >= 0 && y < img->height);
    assert(y + shift < img->height);

    int rowstride = gdk_pixbuf_get_rowstride(img->pixbuf);

    guchar* pixels = gdk_pixbuf_get_pixels(img->pixbuf);
    guchar* src = pixels + (y * rowstride);
    guchar* dst = src + (shift * rowstride);

    // Copy image block into shifted position, but only within image screen area
    int rows = nrRows;
    int availableRows = img->height - y;
    if (rows > availableRows) {
        rows = availableRows;
    }
    int size = rows * rowstride;

    // Use memmove since dst and src are overlapping
    memmove(dst, src, size);

    // Write white pixels on shifted region
    memset(src, 0xff, shift * rowstride);
}

//------------------------------------------------------------------------------------------------------------------------

void
image_setAllPixels(image_t* img, uint8_t channelValue)
{
    assert(img);

    int rowstride = gdk_pixbuf_get_rowstride(img->pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels(img->pixbuf);

    memset(pixels, (int)channelValue, img->height * rowstride);
}

//------------------------------------------------------------------------------------------------------------------------
