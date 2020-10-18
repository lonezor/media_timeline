#include <stdio.h>
#include <dirent.h>
#include <vector>
#include <math.h>
#include <map>
#include <string>
#include <regex>
#include <fstream>
#include <iostream>
#include <mhash.h>
#include <sstream>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "image.hpp"
#include "surface.hpp"

#define TMP_OUTPUT_PATH "/tmp/cg_tmp.png"
#define TMP_OUTPUT_PATH2 "/tmp/cg_tmp2.png"

using dir_name_t = std::string;

typedef enum {
  text_category_main_title, /**< Centered in the middle, large font */
  text_category_title, /**< Centered in the top, small font */
  text_category_timestamp, /** Left-aligned in the top, small font */
  text_category_location, /**< Right-aligned in the top, small font */
  text_category_description, /**< Centered in the button, small font (multi-line) */
} text_category_t;

class fs_scanner
{
    public:
        void scan(std::string path, std::string filter_regex);

        void clear() { path_map_.clear(); }

        std::map<dir_name_t, std::vector<std::string>> path_map() { return path_map_; }

    private:
        std::map<dir_name_t, std::vector<std::string>> path_map_;
};

void fs_scanner::scan(std::string dir_path, std::string filter_regex)
{
    struct dirent **namelist;
	int i,n;

	n = scandir(dir_path.c_str(), &namelist, 0, alphasort);
	if (n < 0)  {
		perror("scandir");
		return;
	}

	for (i = 0; i < n; i++) {
		auto file_name = std::string(namelist[i]->d_name);
		uint8_t type    = namelist[i]->d_type;
		std::string path = dir_path + "/" + file_name;
		size_t pos;

		if (file_name == "." ||
			file_name == "..") {
			free(namelist[i]);
			continue;
		}

		if (type == DT_REG) {
            auto re = std::regex(filter_regex);
            std::smatch match;
            auto matched = std::regex_search(path, match, re);
            if (!matched) {
                continue;
            }
            path_map_[dir_path].emplace_back(path);
		}
		else if (type == DT_DIR) {
			scan(path, filter_regex);
		}
		free(namelist[i]);
	}
	free(namelist);
}

bool file_exists(std::string path)
{
    struct stat sb;
	bool found;
	int result;

    result = stat(path.c_str(), &sb);
    found = (result == 0);

    return found;
}

struct file_description
{
    std::string main_title;
    std::string title;
    std::string timestamp;
    std::string location;
    std::string description;
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

file_description parse_info_file(std::string path)
{
    file_description desc;

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
        auto value = extract_value_from_attribute_pattern(line, "MainTitle: ");
        if (!value.empty()) {
            desc.main_title = value;
            continue;
        }

        // Title
        value = extract_value_from_attribute_pattern(line, "Title: ");
        if (!value.empty()) {
            desc.title = value;
            continue;
        }

        // Title
        value = extract_value_from_attribute_pattern(line, "Time.Date: ");
        if (!value.empty()) {
            desc.timestamp = value;
            continue;
        }

        // Location
        value = extract_value_from_attribute_pattern(line, "Location: ");
        if (!value.empty()) {
            desc.location = value;
            continue;
        }

        // Description
        value = extract_value_from_attribute_pattern(line, "MediaDescription: ");
        if (!value.empty()) {
            desc.description = value;
            continue;
        }

        // Everything else is continuation of description
        if (!value.empty()) {
          desc.description += "\n";
         desc.description += line;
        }
    }

   // printf("desc.main_title '%s'\n", desc.main_title.c_str());
   // printf("desc.title '%s'\n", desc.title.c_str());
   // printf("desc.timestamp '%s'\n", desc.timestamp.c_str());
   // printf("desc.location '%s'\n", desc.location.c_str());
 //   printf("desc.description '%s'\n", desc.description.c_str());

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

void surface_draw_text(surface_t* surface, double font_size, double txt_x, double txt_y, std::string text)
{
#if 1
    surface_op_fontSize(surface, font_size);
    surface_op_sourceRgb(surface, 1, 1, 1);
    surface_op_moveTo(surface, txt_x, txt_y);
    surface_op_showText(surface, text.c_str());
    surface_op_stroke(surface);
#else
    surface_op_fontSize(surface, font_size);
    surface_op_moveTo (surface, txt_x, txt_y);
    surface_op_textPath (surface, text.c_str());
    surface_op_sourceRgb(surface, 1, 1, 1);
    surface_op_fillPreserve (surface);
    surface_op_sourceRgb (surface, 0.15, 0.15, 0.15);
    surface_op_setLineWidth(surface, 0.5);
    surface_op_stroke (surface);
#endif
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
            double txt_offset = (text.size() / 2) * (font_size * 1.25);
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
                if (txt_y < 0) {
                    txt_y = 0;
                }
                surface_draw_text(surface, font_size, txt_x, txt_y, line);
            }
            break;
        }
    }


}



void generate_tagged_image(std::string img_path, file_description file_desc, double screen_width, double screen_height, std::string tagged_img_path) {
    image_t* img = image_create();
    image_loadFromFile(img, img_path.c_str(), screen_width, screen_height);
    image_saveAsPng(img, TMP_OUTPUT_PATH);

    surface_t* surface = surface_create(TMP_OUTPUT_PATH, ANTI_ALIAS_MODE_BEST);
    surface_set_referenceResolution(surface, screen_width, screen_height);
    surface_set_scalingMode(surface, SCALING_MODE_REFERENCE);

    // Main title
    if (!file_desc.main_title.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_main_title, file_desc.main_title);
    }

    // Title
    if (!file_desc.title.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_title, file_desc.title);
    }

    // Timestamp
    if (!file_desc.timestamp.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_timestamp, file_desc.timestamp);
    }

    // Location
    if (!file_desc.location.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_location, file_desc.location);
    }

    // Description
    if (!file_desc.description.empty()) {
        draw_text(surface, screen_width, screen_height, text_category_description, file_desc.description);
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

    //printf("nr_images %d nr_cols %d nr_rows %d thumbnail_width %lf thumbnail_height %lf\n",
      //  nr_images, nr_cols, nr_rows, thumbnail_width, thumbnail_height);

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
                //printf("y_offset %lf x_offset %lf w %lf h %lf\n", y_offset, x_offset, thumbnail_width, thumbnail_height);
                draw_thumbnail(paths[path_idx],  y_offset, x_offset, thumbnail_width, thumbnail_height, surface);
            }

            x_offset += thumbnail_width;
            path_idx++;
        }

        y_offset += thumbnail_height;
    }

    surface_saveAsJpg(surface, out_path1.c_str());

    surface_op_sourceRgba(surface, 0, 0, 0, 0.60);
    surface_op_rectangle(surface, 0, 0, screen_width, screen_height);
    surface_op_fill(surface);

    draw_text(surface, screen_width, screen_height, text_category_main_title, main_title);

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

            auto file_desc = parse_info_file(info_file_path);
            
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


void process_directories(std::map<dir_name_t, std::vector<std::string>>& path_map, std::string root_path)
{

    std::map<std::string,std::vector<std::string>> collage_map;

    // Populate collage map
    for(auto&& it : path_map) {
        auto dir = it.first;

        auto root_path_size = root_path.size();
        dir = dir.substr(root_path_size,dir.size() - root_path_size);

        std::stringstream input(dir);
        std::string e;
        std::string level = root_path;
        
        while (std::getline(input, e, '/')) {
            if (e.empty()) {
                continue;
            }
            level += "/" + e;

            bool first_round = false;
            if (collage_map.find(level) == collage_map.end()) {
                first_round = true;
            }

            fs_scanner scanner;
            scanner.scan(level, ".*hash");
            auto path_map = scanner.path_map();
            for(auto&& it : path_map) {
                auto d = it.first;
                auto p_map = path_map[d];
                for(auto hash_path : p_map) {
                    auto img_base_path = determine_image_base_path_from_hash_file("/home/output", hash_path);
                    auto img_path = img_base_path + "_1080" + ".jpg";
                    if (first_round) {
                        collage_map[level].emplace_back(img_path);
                    } else {
                    }
                }
            }

        }
    }

    // Process collage map
    for(auto&& it : collage_map) {
        auto level = it.first;

        auto pos = level.rfind("/");
        auto main_title = level.substr(pos+1, level.size() - pos);
        pos = main_title.find("_");

        // Remove sorting prefix if used (fixed length format 'xy_')
        if (pos == 2) {
            main_title = main_title.substr(pos+1, main_title.size() - pos);
        }

        auto level_sha1_base = "/home/output/" + string_to_sha1(level);
        auto dir_out_path1 = level_sha1_base + "_1080.jpg";
        auto dir_out_path2 = level_sha1_base + "_1080_tagged.jpg";

        if (!file_exists(dir_out_path1) && 
            !file_exists(dir_out_path2)) {
            generate_collage(1920,
                               1080,
                              collage_map[level],
                              dir_out_path1,
                             dir_out_path2,
                             main_title);
            }
    }
}

int main()
{
    system("/usr/bin/mtransc");

    fs_scanner scanner;
    scanner.scan("/home/lonezor/Media/media_timeline", ".*hash");
    auto path_map = scanner.path_map();

    process_individual_files(path_map);

    scanner.clear();
    scanner.scan("/home/lonezor/Media/media_timeline", "");
    path_map = scanner.path_map();

    process_directories(path_map, "/home/lonezor/Media/media_timeline");


//generate_tagged_video();




    return 0;
}
