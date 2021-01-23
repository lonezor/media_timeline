#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <math.h>
#include <map>
#include <string>
#include <regex>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <mhash.h>
#include <sstream>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "image.hpp"
#include "surface.hpp"
#include "fs_scanner.hpp"

#define TMP_OUTPUT_PATH "/tmp/cg_tmp.png"
#define TMP_OUTPUT_PATH2 "/tmp/cg_tmp2.png"

typedef enum {
  text_category_main_title, /**< Centered in the middle, large font */
  text_category_title, /**< Centered in the top, small font */
  text_category_timestamp, /** Left-aligned in the top, small font */
  text_category_location, /**< Right-aligned in the top, small font */
  text_category_description, /**< Centered in the button, small font (multi-line) */
} text_category_t;

int g_text_offset = 0;

bool file_exists(std::string path)
{
    struct stat sb;
	bool found;
	int result;

    result = stat(path.c_str(), &sb);
    found = (result == 0);

    return found;
}

enum class text_visibility
{
    black,
    white,
};

struct file_description
{
    bool video;
    std::string main_title;
    std::string title;
    std::string timestamp;
    std::string location;
    std::string description;
    text_visibility visibility_main_title;
    text_visibility visibility_title;
    text_visibility visibility_timestamp;
    text_visibility visibility_location;
    text_visibility visibility_description;
    bool shadow_title;
    bool shadow_timestamp;
    bool shadow_location;
    bool shadow_description;
};

std::string extract_value_from_attribute_pattern(const std::string& line, std::string pattern)
{
    size_t line_size = line.size();
    size_t pattern_size = pattern.size();
    std::string value;

    size_t pos = line.find(pattern);
    if (pos != std::string::npos) {
        value = line.substr(pos+pattern_size, line_size - pattern_size);
    }

    return value;
}

file_description parse_info_file(file_description& desc, std::string path)
{
    printf("info file path '%s'\n", path.c_str());

    // Read file
    std::stringstream input;

    try {
        std::ifstream ifs(path);
        input << ifs.rdbuf();
    } catch (const std::ifstream::failure& e) {
        throw;
    }

    // Parse
    std::string line;
    while (std::getline(input, line, '\n')) {

        // Main title
        auto value = extract_value_from_attribute_pattern(line, "W_MainTitle: ");
        if (!value.empty()) {
            if (desc.main_title.empty()) {
                desc.main_title = value;
                desc.visibility_main_title = text_visibility::white;
            }
            continue;
        }

        // Title
        value = extract_value_from_attribute_pattern(line, "W_Title: ");
        if (!value.empty()) {
            if (desc.title.empty()) {
                desc.title = value;
                desc.visibility_title = text_visibility::white;
                desc.shadow_title = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "B_Title: ");
        if (!value.empty()) {
            if (desc.title.empty()) {
                desc.title = value;
                desc.visibility_title = text_visibility::black;
                desc.shadow_title = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "WS_Title: ");
        if (!value.empty()) {
            if (desc.title.empty()) {
                desc.title = value;
                desc.visibility_title = text_visibility::white;
                desc.shadow_title = true;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "BS_Title: ");
        if (!value.empty()) {
            if (desc.title.empty()) {
                desc.title = value;
                desc.visibility_title = text_visibility::black;
                desc.shadow_title = true;
            }
            continue;
        }

        // Timestamp
        value = extract_value_from_attribute_pattern(line, "W_Time.Date: ");
        if (!value.empty()) {
            if (desc.timestamp.empty()) {
                desc.timestamp = value;
                desc.visibility_timestamp = text_visibility::white;
                desc.shadow_timestamp = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "B_Time.Date: ");
        if (!value.empty()) {
            if (desc.timestamp.empty()) {
                desc.timestamp = value;
                desc.visibility_timestamp = text_visibility::black;
                desc.shadow_timestamp = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "WS_Time.Date: ");
        if (!value.empty()) {
            if (desc.timestamp.empty()) {
                desc.timestamp = value;
                desc.visibility_timestamp = text_visibility::white;
                desc.shadow_timestamp = true;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "BS_Time.Date: ");
        if (!value.empty()) {
            if (desc.timestamp.empty()) {
                desc.timestamp = value;
                desc.visibility_timestamp = text_visibility::black;
                desc.shadow_timestamp = true;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "Time.Date: ");
        if (!value.empty()) {
            if (desc.timestamp.empty()) {
                desc.timestamp = value;
                desc.visibility_timestamp = text_visibility::white;
                desc.shadow_timestamp = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "Video: ");
        if (!value.empty()) {
            desc.video = true;
            continue;
        }

        // Location
        value = extract_value_from_attribute_pattern(line, "W_Location: ");
        if (!value.empty()) {
            if (desc.location.empty()) {
                desc.location = value;
                desc.visibility_location = text_visibility::white;
                desc.shadow_location = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "B_Location: ");
        if (!value.empty()) {
            if (desc.location.empty()) {
                desc.location = value;
                desc.visibility_location = text_visibility::black;
                desc.shadow_location = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "WS_Location: ");
        if (!value.empty()) {
            if (desc.location.empty()) {
                desc.location = value;
                desc.visibility_location = text_visibility::white;
                desc.shadow_location = true;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "BS_Location: ");
        if (!value.empty()) {
            if (desc.location.empty()) {
                desc.location = value;
                desc.visibility_location = text_visibility::black;
                desc.shadow_location = true;
            }
            continue;
        }

        // Description
        value = extract_value_from_attribute_pattern(line, "W_MediaDescription: ");
        if (!value.empty()) {
            if (desc.description.empty()) {
                desc.description = value;
                desc.visibility_description = text_visibility::white;
                desc.shadow_description = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "B_MediaDescription: ");
        if (!value.empty()) {
            if (desc.description.empty()) {
                desc.description = value;
                desc.visibility_description = text_visibility::black;
                desc.shadow_description = false;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "WS_MediaDescription: ");
        if (!value.empty()) {
            if (desc.description.empty()) {
                desc.description = value;
                desc.visibility_description = text_visibility::white;
                desc.shadow_description = true;
            }
            continue;
        }

        value = extract_value_from_attribute_pattern(line, "BS_MediaDescription: ");
        if (!value.empty()) {
            if (desc.description.empty()) {
                desc.description = value;
                desc.visibility_description = text_visibility::black;
                desc.shadow_description = true;
            }
            continue;
        }

        // Everything else is continuation of description
        if (!value.empty()) {
          desc.description += "\n";
         desc.description += line;
        }
    }

    return desc;
}

std::string determine_image_base_path_from_hash_file(std::string output_path, std::string hash_path)
{
    std::string sha1;

    // Read file
    std::stringstream input;

    try {
        std::ifstream ifs(hash_path);
        input << ifs.rdbuf();
    } catch (const std::ifstream::failure& e) {
        throw;
    }

    // Parse
    std::string line;
    while (std::getline(input, line, '\n')) {
        // Main title
        auto value = extract_value_from_attribute_pattern(line, "sha1: ");
        if (!value.empty()) {
            sha1 = value;
            break;
        }
    }

    return output_path + "/" + sha1;
}

void surface_draw_text(surface_t* surface, double font_size, double txt_x, double txt_y, std::string text, text_visibility visibility, bool shadow)
{
    surface_op_fontFace(surface, "Lato Black", font_slant::normal, font_weight::normal);
    surface_op_fontSize(surface, font_size);


#if 0
    // Background
    if (shadow) {
    //    if (visibility == text_visibility::black) {
      //      surface_op_sourceRgb(surface, 1, 1, 1);
       // } else {
            surface_op_sourceRgb(surface, 0, 0, 0);
       // }
        surface_op_fontSize(surface, font_size * 1.010);
        surface_op_moveTo(surface, txt_x - (txt_x * 0.002), txt_y + (txt_y * 0.05));
        surface_op_showText(surface, text.c_str());
        surface_op_stroke(surface);
    }

    #endif

    // Foreground
    //if (visibility == text_visibility::black) {
   //     surface_op_sourceRgb(surface, 0, 0, 0);
   // } else {
        //surface_op_sourceRgb(surface, 1.0, 0.834, 0.168);
  //      1.0, 0.834, 0.168
  //  }

  


    surface_op_moveTo(surface, txt_x, txt_y);
    surface_op_textPath(surface, text.c_str());
    surface_op_sourceRgb(surface, 1, 1, 1);
    surface_op_fillPreserve(surface);
    surface_op_sourceRgb(surface, 0, 0, 0);
    surface_op_lineWidth(surface, 10);
    surface_op_stroke(surface);



    
    //surface_op_fontSize(surface, font_size);
    ///surface_op_moveTo(surface, txt_x, txt_y);
    //surface_op_showText(surface, text.c_str());
    //surface_op_stroke(surface);
}

void draw_text(surface_t* surface, double screen_width, double screen_height, text_category_t txt_category, std::string text, text_visibility visibility, bool shadow)
{
    double font_size = 1;
    double txt_x = 0;
    double txt_y = 0;

    switch(txt_category) {
        case text_category_title: {
            font_size = 300;
            txt_y = font_size;
            double txt_x = (double)g_text_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }
            surface_draw_text(surface, font_size, txt_x, txt_y, text, visibility, shadow);
            break;
        }
        case text_category_main_title: {
            font_size = 500;
            double txt_offset = (text.size() / 2) * (font_size * 0.61);
            txt_x = (screen_width / 2) - txt_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }

            txt_y = (screen_height / 2) + (font_size * 0.30);
            surface_draw_text(surface, font_size, txt_x, txt_y, text, visibility, shadow);
            break;
        }
        case text_category_timestamp: {
            font_size = 20;
            txt_x = 5;
            txt_y = font_size;
            surface_draw_text(surface, font_size, txt_x, txt_y, text, visibility, shadow);
            break;
        }
        case text_category_location: {
            font_size = 20;
            double txt_offset = text.size() * (font_size * 0.66);
            txt_x = screen_width - txt_offset;
            if (txt_x < 0) {
                txt_x = 0;
            }
            txt_y = font_size;
            surface_draw_text(surface, font_size, txt_x, txt_y, text, visibility, shadow);
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
                if (txt_y < 0) {
                    txt_y = 0;
                }
                surface_draw_text(surface, font_size, txt_x, txt_y, line, visibility, shadow);
            }
            break;
        }
    }


}



void generate_tagged_image(std::string img_path, file_description file_desc, double screen_width, double screen_height, std::string tagged_img_path) {
    image_t* img = image_create();
    image_loadFromFile(img, img_path.c_str(), screen_width, screen_height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);
    image_destroy(img);

    surface_t* surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);
    surface_set_referenceResolution(surface, screen_width, screen_height);
    surface_set_scalingMode(surface, SCALING_MODE_REFERENCE);

    // Main title
    if (!file_desc.main_title.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_main_title, file_desc.main_title, file_desc.visibility_main_title, false);
    }

    // Title
    if (!file_desc.title.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_title, file_desc.title, file_desc.visibility_title, file_desc.shadow_title);
    }

    // Timestamp
    if (!file_desc.timestamp.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_timestamp, file_desc.timestamp, file_desc.visibility_timestamp, file_desc.shadow_timestamp);
    }

    // Location
    if (!file_desc.location.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_location, file_desc.location, file_desc.visibility_location, file_desc.shadow_location);
    }

    // Description
    if (!file_desc.description.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_description, file_desc.description, file_desc.visibility_description, file_desc.shadow_description);
    }
 
    surface_saveAsJpg(surface, tagged_img_path.c_str());

    surface_destroy(surface);
}

void generate_tagged_video()
{
    //ffmpeg -i /home/lonezor/Media/media_timeline/20151224_095247.mp4 -vcodec png /tmp/cg_tmp_dir/output%06d.png

    fs_scanner scanner;
    scanner.scan("/tmp/cg_tmp_dir", ".*png");
    
    file_description file_desc;
    file_desc.timestamp = "2015-12-23";
    file_desc.location = "Funchal, Madeira, Portugal";
    file_desc.description = "abc";

    auto path_map = scanner.path_map();
    for(auto&& it : path_map) {
            auto dir = it.first;
            for(auto&& path : it.second) {
                printf("%s\n", path.c_str());
                generate_tagged_image(path, file_desc, 1920, 1080, path);
            }
           
    }
    
}

void draw_thumbnail(std::string path,  double y_offset, double x_offset,
     double thumbnail_width, double thumbnail_height, surface_t* surface)
{
    image_t* img = image_create();
    image_loadFromFile(img, path.c_str(), thumbnail_width, thumbnail_height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);

    surface_t* img_surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);

    surface_op_draw_image_surface(surface, img_surface, x_offset, y_offset);
}

void generate_collage(double screen_width,
                      double screen_height,
                      const std::vector<std::string>& paths,
                      std::string out_path1,
                      std::string out_path2,
                      std::string main_title)
{
    int nr_images = paths.size();

    printf("generating %s from %d images\n", out_path2.c_str(), nr_images);

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
    surface_set_scalingMode(surface, SCALING_MODE_NONE);

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
               // printf("y_offset %lf x_offset %lf w %lf h %lf\n", y_offset, x_offset, thumbnail_width, thumbnail_height);
                draw_thumbnail(paths[path_idx],  y_offset, x_offset, thumbnail_width, thumbnail_height, surface);
            }

            x_offset += thumbnail_width;
            path_idx++;
        }

        y_offset += thumbnail_height;
    }

    //surface_saveAsJpg(surface, out_path1.c_str());

  //  surface_op_sourceRgba(surface, 0, 0, 0, 0.60);
  //  surface_op_rectangle(surface, 0, 0, screen_width, screen_height);
  //  surface_op_fill(surface);

    draw_text(surface, screen_width, screen_height, text_category_title, main_title, text_visibility::white, true);

    surface_saveAsJpg(surface, out_path2.c_str());

    surface_destroy(surface);
    image_destroy(img);
}

void process_individual_files(std::map<dir_name_t, std::vector<std::string>>& path_map)
{
    for(auto&& it : path_map) {
        auto dir = it.first;
        auto paths = path_map[dir];
        for(auto hash_path : paths) {

            auto img_base_path = determine_image_base_path_from_hash_file("/home/output", hash_path);
            auto info_file_path = img_base_path + ".info";

            // Overriding top level info file
            file_description file_desc;
            file_desc = parse_info_file(file_desc, hash_path);

            // Extend with detailed info file
            file_desc = parse_info_file(file_desc, info_file_path);

            if (file_desc.timestamp.find("empty") != std::string::npos) {
                file_desc.timestamp = "";
            }
            
            auto quality_suffix = "1080";
            auto img_path = img_base_path + "_" + quality_suffix  +  ".jpg";
            auto tagged_img_path = img_base_path + "_" + quality_suffix + "_tagged.jpg";

            if (!file_exists(tagged_img_path)) {
                generate_tagged_image(img_path, file_desc, 1920, 1080, tagged_img_path);
            }

            //auto 
            // parse .descr
            // parse .hash -> image file

            //printf("%s: %s\n", dir.c_str(), path.c_str());

        }
    }
}

// O(1) uint8 to hex string: 0x5b -> "5b"
constexpr int bin_to_str_array_size = 256;
static constexpr const std::array<const char*, bin_to_str_array_size> bin_to_str{
    { "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b", "0c", "0d", "0e", "0f", "10", "11", "12",
      "13", "14", "15", "16", "17", "18", "19", "1a", "1b", "1c", "1d", "1e", "1f", "20", "21", "22", "23", "24", "25",
      "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f", "30", "31", "32", "33", "34", "35", "36", "37", "38",
      "39", "3a", "3b", "3c", "3d", "3e", "3f", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4a", "4b",
      "4c", "4d", "4e", "4f", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e",
      "5f", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b", "6c", "6d", "6e", "6f", "70", "71",
      "72", "73", "74", "75", "76", "77", "78", "79", "7a", "7b", "7c", "7d", "7e", "7f", "80", "81", "82", "83", "84",
      "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f", "90", "91", "92", "93", "94", "95", "96", "97",
      "98", "99", "9a", "9b", "9c", "9d", "9e", "9f", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "aa",
      "ab", "ac", "ad", "ae", "af", "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd",
      "be", "bf", "c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb", "cc", "cd", "ce", "cf", "d0",
      "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "da", "db", "dc", "dd", "de", "df", "e0", "e1", "e2", "e3",
      "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef", "f0", "f1", "f2", "f3", "f4", "f5", "f6",
      "f7", "f8", "f9", "fa", "fb", "fc", "fd", "fe", "ff" }
};

std::string
buffer_to_hex_string(std::vector<uint8_t> buffer) noexcept
{
    std::string hex;
    hex.reserve(buffer.size() * 2 + 1);

    // For each octet in the buffer, use lookup table and
    // retrieve its hex representation
    for (auto const& i : buffer) {
        hex += std::string(bin_to_str.at(i));
    }

    return hex;
}

std::string string_to_sha1(std::string input)
{
	MHASH td;
    std::vector<uint8_t> buffer;
    buffer.resize(20);

	td = mhash_init(MHASH_SHA1);
	if (td == MHASH_FAILED) {
        throw;
	}

    mhash(td, input.data(), input.size());

	mhash_deinit(td, buffer.data()); /* hash value is now accessible */

    return buffer_to_hex_string(buffer);
}



int main(int argc, const char* argv[])
{
    if (argc < 4) {
        return 0;
    }

    std::string title = std::string(argv[1]);

    std::string path = std::string(argv[2]);

    g_text_offset = atoi(argv[3]);

    int width = 7680;
    int height = 4320;

    printf("using '%s' and title '%s', text offset %d\n", path.c_str(), title.c_str(), g_text_offset);

    image_t* img = image_create();
    image_loadFromFile(img, path.c_str(), width, height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);

    surface_t* surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);
    surface_set_scalingMode(surface, SCALING_MODE_NONE);

    surface_op_sourceRgb(surface, 0, 0, 0);
    surface_op_rectangle(surface, 0, 0, width, height);
    surface_op_fill(surface);   

    surface_t* img_surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);
    surface_op_draw_image_surface(surface, img_surface, 0, 0);
    surface_destroy(img_surface);

    draw_text(surface, width, height, text_category_title, title, text_visibility::white, true);

    surface_saveAsJpg(surface, "/tmp/tagged_img.png");

    surface_destroy(surface);
    image_destroy(img);

    return 0;
}
