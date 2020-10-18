#include <vector>
#include <iostream>
#include <sstream>
#include <cmath>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <errno.h>

#include "image.hpp"
#include "surface.hpp"

#define TMP_OUTPUT_PATH "/tmp/cg_tmp.png"

void draw_thumbnail(std::string path,  double y_offset, double x_offset, double thumbnail_width, double thumbnail_height, surface_t* surface)
{
    image_t* img = image_create();
    image_loadFromFile(img, path.c_str(), thumbnail_width, thumbnail_height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);

    surface_t* img_surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);

    surface_op_draw_image_surface(surface, img_surface, x_offset, y_offset);
}

typedef enum {
  text_category_main_title, /**< Centered in the middle, large font */
  text_category_title, /**< Centered in the top, small font */
  text_category_timestamp, /** Left-aligned in the top, small font */
  text_category_location, /**< Right-aligned in the top, small font */
  text_category_description, /**< Centered in the button, small font (multi-line) */
} text_category_t;

void surface_draw_text(surface_t* surface, double font_size, double txt_x, double txt_y, std::string text)
{
    printf("x %f y %f font size %f\n", txt_x, txt_y, font_size);

    surface_op_fontSize(surface, font_size);
    surface_op_sourceRgb(surface, 1, 1, 1);
    surface_op_moveTo(surface, txt_x, txt_y);
    surface_op_showText(surface, text.c_str());
    surface_op_stroke(surface);
}

void draw_text(surface_t* surface, double screen_width, double screen_height, text_category_t txt_category, std::string text)
{
    double font_size = 1;
    double txt_x = 0;
    double txt_y = 0;

    switch(txt_category) {
        case text_category_title: {
            font_size = 20;
            txt_y = font_size;
            double txt_offset = (text.size() / 2) * (font_size * 1.16);
            txt_x = (screen_width / 2) - txt_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }
            surface_draw_text(surface, font_size, txt_x, txt_y, text);
            break;
        }
        case text_category_main_title: {
            font_size = 100;
            double txt_offset = (text.size() / 2) * (font_size * 0.61);
            txt_x = (screen_width / 2) - txt_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }

            txt_y = (screen_height / 2) + (font_size * 0.30);
            surface_draw_text(surface, font_size, txt_x, txt_y, text);
            break;
        }
        case text_category_timestamp: {
            font_size = 20;
            txt_x = 5;
            txt_y = font_size;
            surface_draw_text(surface, font_size, txt_x, txt_y, text);
            break;
        }
        case text_category_location: {
            font_size = 20;
            double txt_offset = (text.size() / 2) * (font_size * 1.16);
            txt_x = screen_width - txt_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }
            txt_y = font_size;
            surface_draw_text(surface, font_size, txt_x, txt_y, text);
            break;
        }
        case text_category_description: {
            std::stringstream input(text);
            std::string line;
            std::vector<std::string> lines;
            while (std::getline(input, line, '\n')) {
                lines.push_back(line);
            }

            font_size = 25;
            

            auto nr_lines = lines.size();
            int j = nr_lines;
            for(int i=0; i < nr_lines ; i++, j--) {
                auto line = lines[i];
                double txt_offset = (line.size() / 2) * (font_size * 0.61);
                txt_x = (screen_width / 2) - txt_offset;
                txt_y = screen_height - (j * 1.25 * font_size) + (20);
                printf("txt_y %f\n", txt_y);
                if (txt_y < 0) {
                    txt_y = 0;
                }
                surface_draw_text(surface, font_size, txt_x, txt_y, line);
            }
            break;
        }
    }


}


void generate_collage(double screen_width, double screen_height, const std::vector<std::string>& paths, std::string out_path)
{
    int nr_images = paths.size();

    if (nr_images == 0) {
        return;
    }

    // Determine suitable grid
    constexpr int min_cols = 2;

    double aspect_ratio = screen_width / screen_height;

    int nr_cols = ceil(sqrt(static_cast<double>(nr_images)));
    if (nr_cols < min_cols) {
        nr_cols = min_cols;
    }

    int nr_rows = ceil(static_cast<double>(nr_images) / static_cast<double>(nr_cols));


    double thumbnail_width  = floor(screen_width / static_cast<double>(nr_cols));
    double thumbnail_height = floor(thumbnail_width / aspect_ratio);

    printf("nr_images %d nr_cols %d nr_rows %d thumbnail_width %lf thumbnail_height %lf\n",
        nr_images, nr_cols, nr_rows, thumbnail_width, thumbnail_height);

    // Create surface
    image_t* img = image_create();
    image_loadFromFile(img, paths[0].c_str(), screen_width, screen_height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);

    surface_t* surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);
    surface_set_scalingMode(surface, SCALING_MODE_REFERENCE);

    surface_op_sourceRgb(surface, 0, 0, 0);
    surface_op_rectangle(surface, 0, 0, screen_width, screen_height);
    surface_op_fill(surface);

    // Populate grid
    int path_idx = 0;
    double y_offset = 0;
    for(int y=0; y < nr_rows; y++) {
        double x_offset = 0;
        for(int x=0; x < nr_cols; x++, path_idx) {

            if (path_idx < nr_images) {
                printf("y_offset %lf x_offset %lf w %lf h %lf\n", y_offset, x_offset, thumbnail_width, thumbnail_height);
                draw_thumbnail(paths[path_idx],  y_offset, x_offset, thumbnail_width, thumbnail_height, surface);
            }

            x_offset += thumbnail_width;
            path_idx++;
        }

        y_offset += thumbnail_height;
    }

    surface_saveAsPng(surface, "/home/output/out1.png");

    surface_op_sourceRgba(surface, 0, 0, 0, 0.60);
    surface_op_rectangle(surface, 0, 0, screen_width, screen_height);
    surface_op_fill(surface);

    draw_text(surface, screen_width, screen_height, text_category_title, "Title text 123");
 
 
    draw_text(surface, screen_width, screen_height, text_category_main_title, "Time Period 2005 - 2010");
    draw_text(surface, screen_width, screen_height, text_category_timestamp, "2020-09-26");
    draw_text(surface, screen_width, screen_height, text_category_location, "Olympic park business center, Beijing, China");
   
    draw_text(surface, screen_width, screen_height, text_category_description,
        "Testing this by writing a few sequences of words that\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        "a\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        " dsf dskjfk dsjfkjds kfjkdsj fkjdsk fkdsj fkdjsfkjds\n"
        " fdjfkdjskfjdskj\n"
        " dfjd\n");

    surface_saveAsPng(surface, "/home/output/out2.png");

    surface_destroy(surface);
    image_destroy(img);

}



