#ifndef _SURFACE_H_
#define _SURFACE_H_

#include "image.hpp"
#include <string>

//------------------------------------------------------------------------------------------------------------------------

typedef enum {
    ANTI_ALIAS_MODE_NONE,
    ANTI_ALIAS_MODE_FAST,
    ANTI_ALIAS_MODE_BEST,
} antiAliasMode_t;

typedef enum {
    SCALING_MODE_NONE,      // The same coordinates are used regardless of resolution
    SCALING_MODE_REFERENCE, // Coordinates are adjusted based on reference resolution (default)
} scalingMode_t;

enum class font_slant
{
    normal,
    italic,
    oblique,
};

enum class font_weight
{
    normal,
    bold,
};

typedef struct _surface_t surface_t;

//------------------------------------------------------------------------------------------------------------------------

surface_t*
surface_create(const char* path, antiAliasMode_t antiAliasMode);

void
surface_destroy(surface_t* s);

double
surface_get_screenWidth(surface_t* s);

double
surface_get_screenHeight(surface_t* s);

void
surface_set_referenceResolution(surface_t* s, int width, int height);

void
surface_set_scalingMode(surface_t* s, scalingMode_t scalingMode);

void
surface_op_lineWidth(surface_t* s, double lineWidth);

void
surface_op_fontFace(surface_t* s, std::string name, font_slant slant, font_weight weight);

void
surface_op_fontSize(surface_t* s, double fontSize);

void
surface_op_showText(surface_t* s, const char* text);

void
surface_op_textPath(surface_t* s, const char* text);

void
surface_op_sourceRgb(surface_t* s, double r, double g, double b);

void
surface_op_sourceRgba(surface_t* s, double r, double g, double b, double a);

void
surface_op_paint(surface_t* s);

void
surface_op_moveTo(surface_t* s, double x, double y);

void
surface_op_lineTo(surface_t* s, double x, double y);

void
surface_op_closePath(surface_t* s);

void
surface_op_arc(surface_t* s, double xc, double yc, double radius, double angle1, double angle2);

void
surface_op_fill(surface_t* s);

void
surface_op_stroke(surface_t* s);

void
surface_op_rectangle(surface_t* s, double x, double y, double width, double height);

void
surface_op_draw_image_surface(surface_t* s, surface_t* img_surface, double x, double y);

void
surface_op_clear(surface_t* s);

void
surface_saveAsPng(surface_t* s, const char* path);

void
surface_saveAsJpg(surface_t* s, const char* path);

void
surface_op_fillPreserve(surface_t* s);

void
surface_op_setLineWidth(surface_t* s, double width);

//------------------------------------------------------------------------------------------------------------------------

#endif /* _SURFACE_H_ */
