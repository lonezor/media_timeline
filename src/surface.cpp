#include "surface.hpp"
#include "cairo_png_writer.hpp"
#include "cairo_jpg.h"

#include <assert.h>
#include <cairo.h>
#include <stdio.h>
#include <string.h>

#define COMPONENT LOG_COMPONENT_OFFLOADSERVER_SNAPSHOTANALYZER_SURFACE

//------------------------------------------------------------------------------------------------------------------------

struct _surface_t
{
    double width;              // Current width
    double height;             // Current height
    double refWidth;           // Scaling reference width
    double refHeight;          // Scaling reference height
    scalingMode_t scalingMode; // Indication if coordinate scaling is on/off
    cairo_surface_t* surface;  // Cairo surface
    cairo_t* cr;               // Cairo drawing context
};

//-----------------------------------------------------------------------------------------------------------

static double
surface_scale(surface_t* s, double value);

//-----------------------------------------------------------------------------------------------------------

surface_t*
surface_create(const char* path, antiAliasMode_t antiAliasMode)
{
    surface_t* s = (surface_t*)calloc(1, sizeof(surface_t));
    assert(s);

    s->surface = cairo_image_surface_create_from_png(path);

    s->width = cairo_image_surface_get_width (s->surface);
    s->height = cairo_image_surface_get_height (s->surface);

    if (!s->width || !s->height)
        return NULL;

    s->cr = cairo_create(s->surface);
    assert(s->cr);

    switch (antiAliasMode) {
        case ANTI_ALIAS_MODE_NONE:
            cairo_set_antialias(s->cr, CAIRO_ANTIALIAS_NONE);
            break;
        case ANTI_ALIAS_MODE_FAST:
            cairo_set_antialias(s->cr, CAIRO_ANTIALIAS_FAST);
            break;
        case ANTI_ALIAS_MODE_BEST:
        default:
            cairo_set_antialias(s->cr, CAIRO_ANTIALIAS_BEST);
            break;
    }

    cairo_select_font_face(s->cr,
                           "DejaVu Sans Book",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD); // This could be exposed by the API

    s->refWidth = 1920;
    s->refHeight = 1080;
    s->scalingMode = SCALING_MODE_REFERENCE;

    return s;
}

//-----------------------------------------------------------------------------------------------------------

void surface_op_fontFace(surface_t* s, std::string name, font_slant slant, font_weight weight)
{
    cairo_font_slant_t cr_slant;
    switch(slant)
    {
        case font_slant::normal:
            cr_slant = CAIRO_FONT_SLANT_NORMAL;
            break;
        case font_slant::italic:
            cr_slant = CAIRO_FONT_SLANT_ITALIC;
            break;
        case font_slant::oblique:
            cr_slant = CAIRO_FONT_SLANT_OBLIQUE;
            break;
    }

    cairo_font_weight_t cr_weight;
    switch(weight)
    {
        case font_weight::normal:
            cr_weight = CAIRO_FONT_WEIGHT_NORMAL;
            break;
        case font_weight::bold:
            cr_weight = CAIRO_FONT_WEIGHT_BOLD;
            break;
    }
    
    auto cr = s->cr;
    cairo_select_font_face(cr, name.c_str(), cr_slant, cr_weight);
}

//-----------------------------------------------------------------------------------------------------------



//-----------------------------------------------------------------------------------------------------------

void
surface_destroy(surface_t* s)
{
    assert(s);

    cairo_destroy(s->cr);
    cairo_surface_destroy(s->surface);
    free(s);
}

//-----------------------------------------------------------------------------------------------------------

double
surface_get_screenWidth(surface_t* s)
{
    return s->width;
}

//-----------------------------------------------------------------------------------------------------------

double
surface_get_screenHeight(surface_t* s)
{
    assert(s);
    return s->height;
}

//-----------------------------------------------------------------------------------------------------------

void
surface_set_referenceResolution(surface_t* s, int width, int height)
{
    assert(s);
    s->refWidth = (double)width;
    s->refHeight = (double)height;
}

//-----------------------------------------------------------------------------------------------------------

void
surface_set_scalingMode(surface_t* s, scalingMode_t scalingMode)
{
    assert(s);
    s->scalingMode = scalingMode;
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_lineWidth(surface_t* s, double lineWidth)
{
    assert(s);
    cairo_set_line_width(s->cr, surface_scale(s, lineWidth));
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_fontSize(surface_t* s, double fontSize)
{
    assert(s);
    cairo_set_font_size(s->cr, surface_scale(s, fontSize));
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_showText(surface_t* s, const char* text)
{
    assert(s);
    cairo_show_text(s->cr, text);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_sourceRgb(surface_t* s, double r, double g, double b)
{
    assert(s);
    cairo_set_source_rgb(s->cr, r, g, b);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_sourceRgba(surface_t* s, double r, double g, double b, double a)
{
    assert(s);
    cairo_set_source_rgba(s->cr, r, g, b, a);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_paint(surface_t* s)
{
    assert(s);
    cairo_paint(s->cr);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_moveTo(surface_t* s, double x, double y)
{
    assert(s);
    cairo_move_to(s->cr, surface_scale(s, x), surface_scale(s, y));
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_lineTo(surface_t* s, double x, double y)
{
    assert(s);
    cairo_line_to(s->cr, surface_scale(s, x), surface_scale(s, y));
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_closePath(surface_t* s)
{
    assert(s);
    cairo_close_path(s->cr);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_arc(surface_t* s, double xc, double yc, double radius, double angle1, double angle2)
{
    assert(s);
    cairo_arc(s->cr, surface_scale(s, xc), surface_scale(s, yc), surface_scale(s, radius), angle1, angle2);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_fill(surface_t* s)
{
    assert(s);
    cairo_fill(s->cr);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_stroke(surface_t* s)
{
    assert(s);
    cairo_stroke(s->cr);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_rectangle(surface_t* s, double x, double y, double width, double height)
{
    assert(s);
    cairo_rectangle(s->cr, surface_scale(s, x), surface_scale(s, y), surface_scale(s, width), surface_scale(s, height));
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_draw_image_surface(surface_t* s, surface_t* img_surface, double x, double y)
{
    cairo_set_source_surface (s->cr, img_surface->surface, surface_scale(s, x), surface_scale(s, y));
    cairo_paint(s->cr);
}

//-----------------------------------------------------------------------------------------------------------

void
surface_op_clear(surface_t* s)
{
    assert(s);
    cairo_set_source_rgb(s->cr, 0, 0, 0);
    cairo_paint(s->cr);
}

void
surface_op_textPath(surface_t* s, const char* text)
{
    cairo_text_path (s->cr, text);
}

void
surface_op_fillPreserve(surface_t* s)
{
    cairo_fill_preserve(s->cr);
}

void
surface_op_setLineWidth(surface_t* s, double width)
{
    cairo_set_line_width (s->cr, width);
}

//-----------------------------------------------------------------------------------------------------------

static double
surface_scale(surface_t* s, double value)
{
    assert(s);

    if (s->scalingMode == SCALING_MODE_NONE)
        return value;

    // When scaling between resolutions with the same aspect ratio
    // width and height have the same proportions against the
    // reference. Width used for the calculation.
    double multiplier = s->width / s->refWidth;
    return value * multiplier;
}

//-----------------------------------------------------------------------------------------------------------

void
surface_saveAsPng(surface_t* s, const char* path)
{
    assert(s);

    printf("surface_saveAsPng %s\n", path);

    // Use minimum compression to reduce CPU load.
    // The compression level must not be zero since
    // the file size is used to filter out identical
    // images and thus reduces the need to analyze
    // the majority of the images
    cairoPngWriter_writeToFileSystem(s->surface, path, 1);
}

void
surface_saveAsJpg(surface_t* s, const char* path)
{
    assert(s);

    printf("surface_saveAsJpg %s\n", path);

    cairo_image_surface_write_to_jpeg(s->surface, path, 95);
}



//-----------------------------------------------------------------------------------------------------------
