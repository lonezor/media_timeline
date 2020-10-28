#include "fs_scanner.hpp"

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include <mhash.h>


#include <stdlib.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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

struct timeline_entry
{
    bool directory;
    bool video;
    std::string raw_path;
    std::string tagged_path;
    std::string dir_path;
    int level;
};

timeline_entry create_timeline_entry(bool directory, bool video, std::string dir_path, int level, std::string raw_path, std::string tagged_path)
{
    timeline_entry entry;

    entry.directory = directory;
    entry.video = video;
    entry.dir_path = dir_path;
    entry.level = level;
    entry.raw_path = raw_path;
    entry.tagged_path = tagged_path;

    return entry;
}

int get_level_integer(std::string level)
{
    int lvl = 0;
    for(auto&& c : level) {
        if (c == '/') {
            lvl++;
        }
    }

    return lvl;
}

std::vector<timeline_entry> discover_timeline_entries(std::string root_path) {
    std::vector<timeline_entry> timeline_entries;
    fs_scanner scanner;
    scanner.scan(root_path, ".*info");

    auto path_map = scanner.path_map();

    std::map<std::string,bool> found_dirs;

    for(auto&& it : path_map) {
        auto dir = it.first;

        auto root_path_size = root_path.size();
        dir = dir.substr(root_path_size,dir.size() - root_path_size);

        std::stringstream input(dir);
        std::string e;
        std::string level = root_path;
        std::string dir_name;
        
        while (std::getline(input, e, '/')) {
            if (e.empty()) {
                continue;
            }
            dir_name = level;
            level += "/" + e;

            if (found_dirs.find(level) == found_dirs.end()) {
                printf("Scanning '%s'\n", level.c_str());

                auto level_sha1_base = "/home/output/" + string_to_sha1(level);
                auto dir_raw_path = level_sha1_base + "_1080.jpg";
                auto dir_tagged_path = level_sha1_base + "_1080_tagged.jpg";

                auto lvl = get_level_integer(level);

                timeline_entries.emplace_back(create_timeline_entry(true, false, dir_name, lvl, dir_raw_path, dir_tagged_path));

                scanner.clear();
                scanner.set_recursive(false);
                scanner.scan(level, ".*info");
                auto files = scanner.path_map();

                if (files.size() == 0) {
                    found_dirs[level] = true;
                    continue;
                }

                for(auto&& level_it : files) {
                     auto level_dir = level_it.first;
                     auto p_map = path_map[level_dir];
                    for(auto hash_path : p_map) {
                        auto img_base_path = determine_image_base_path_from_hash_file("/home/output", hash_path);
                        auto img_raw_path = img_base_path + "_1080.jpg";
                        auto img_tagged_path = img_base_path + "_1080_tagged.jpg";

                        timeline_entries.emplace_back(create_timeline_entry(false, false, dir_name, lvl, img_raw_path, img_tagged_path));
                    }
                }
            }
            
            found_dirs[level] = true;

        }
    }

    return timeline_entries;
}

void display_image(timeline_entry& te, SDL_Renderer *renderer, SDL_Texture** texture, bool toggle_flag)
{
    if (*texture != NULL) {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }

    const char* path;
    if (toggle_flag == 0) {
        path = te.tagged_path.c_str();
    } else {
        path = te.raw_path.c_str();
    }

    printf("Loading %s\n", path);

    *texture = IMG_LoadTexture(renderer, path);
    
    SDL_RenderCopy(renderer, *texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int find_prev_dir_idx(std::vector<timeline_entry>& timeline_entries, int curr_idx)
{
    int idx = curr_idx;
    int curr_lvl = timeline_entries[curr_idx].level;

    while(idx - 1 >= 0) {
        idx--;

        if (timeline_entries[idx].directory &&
            timeline_entries[idx].level == curr_lvl &&
            timeline_entries[idx].dir_path.find(timeline_entries[curr_idx].dir_path) != std::string::npos) {
            return idx;
        }
    }

    return curr_idx;
}

int find_next_dir_idx(std::vector<timeline_entry>& timeline_entries, int curr_idx)
{
    int idx = curr_idx;
    int curr_lvl = timeline_entries[curr_idx].level;

    while(idx + 1 < timeline_entries.size()) {
        idx++;

        if (timeline_entries[idx].directory && 
            timeline_entries[idx].level == curr_lvl &&
            timeline_entries[idx].dir_path.find(timeline_entries[curr_idx].dir_path) != std::string::npos) {
            return idx;
        }
    }

    return curr_idx;
}

int find_prev_level_idx(std::vector<timeline_entry>& timeline_entries, int curr_idx)
{
    int curr_lvl = timeline_entries[curr_idx].level;
    int idx = curr_idx;

    while(idx - 1 >= 0) {
        idx--;

        if (timeline_entries[idx].level < curr_lvl &&
            timeline_entries[idx].directory) {
            return idx;
        }
    }

    return curr_idx;
}

int find_next_level_idx(std::vector<timeline_entry>& timeline_entries, int curr_idx)
{
    int curr_lvl = timeline_entries[curr_idx].level;
    int idx = curr_idx;

    while(idx + 1 < timeline_entries.size()) {
        idx++;

        if (timeline_entries[idx].level > curr_lvl &&
            timeline_entries[idx].directory) {
            return idx;
        }
    }

    return curr_idx;
}

int main(int argc, char* argv[])
{
    std::string root_path = "/home/lonezor/Media/media_timeline";

    auto timeline_entries = discover_timeline_entries(root_path);

    int nr_te_entries = timeline_entries.size();
    
    // We want to display the latest entry to start with
    int te_idx = nr_te_entries-1;

    timeline_entry te = timeline_entries[te_idx];

    bool toggle_flag = false;

    SDL_Event event;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Window *window = NULL;

    SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(
        1920, 1080,
        0, &window, &renderer
    );

    IMG_Init(IMG_INIT_JPG);

    display_image(te, renderer, &texture, toggle_flag);

    bool exit_signal = false;

    SDL_Keycode prev_key_code = 0;

    while(!exit_signal){
        while (SDL_PollEvent( &event ) && !exit_signal) {
            if (event.type == SDL_KEYDOWN) {
                auto key_code = event.key.keysym.sym;
                if (key_code != prev_key_code) {
                    // Reset toggle flag
                    if (key_code != SDLK_SPACE) {
                        toggle_flag = false;
                    }

                    int shift_state = KMOD_SHIFT;
                    bool shift = false;
                    if (SDL_GetModState() & shift_state != 0) {
                        shift = true;
                    }

                    switch (key_code) {
                        case SDLK_q:
                            exit_signal = true;
                            break;
                        case SDLK_f: {
                            static int mode = 0;
                            if (mode == 0) {
                                mode = SDL_WINDOW_FULLSCREEN_DESKTOP;
                            } else {
                                mode = 0;
                            }
                            SDL_SetWindowFullscreen(window, mode);
                            display_image(te, renderer, &texture, toggle_flag);
                            break;
                        }
                        case SDLK_SPACE:
                            toggle_flag = !toggle_flag;
                            display_image(te, renderer, &texture, toggle_flag);
                            break;
                        case SDLK_LEFT:
                            if (shift) {
                                auto idx = find_prev_dir_idx(timeline_entries, te_idx);
                                if (idx != te_idx) {
                                    te_idx = idx;
                                    te = timeline_entries[te_idx];
                                    display_image(te, renderer, &texture, toggle_flag);
                                }
                            } else {
                                if (te_idx - 1 >= 0)  {
                                    te_idx--;
                                }
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        case SDLK_RIGHT:
                            if (shift) {
                                auto idx = find_next_dir_idx(timeline_entries, te_idx);
                                if (idx != te_idx) {
                                    te_idx = idx;
                                    te = timeline_entries[te_idx];
                                    display_image(te, renderer, &texture, toggle_flag);
                                }
                            } else {
                                if (te_idx + 1 < nr_te_entries)  {
                                    te_idx++;
                                }
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        case SDLK_UP: {
                            auto idx = find_prev_level_idx(timeline_entries, te_idx);
                            if (idx != te_idx) {
                                te_idx = idx;
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        }
                        case SDLK_DOWN: {
                            auto idx = find_next_level_idx(timeline_entries, te_idx);
                            if (idx != te_idx) {
                                te_idx = idx;
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        }
                        case SDLK_PAGEUP: {
                            auto idx = find_prev_dir_idx(timeline_entries, te_idx);
                            if (idx != te_idx) {
                                te_idx = idx;
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        }
                        case SDLK_PAGEDOWN: {
                            auto idx = find_next_dir_idx(timeline_entries, te_idx);
                            if (idx != te_idx) {
                                te_idx = idx;
                                te = timeline_entries[te_idx];
                                display_image(te, renderer, &texture, toggle_flag);
                            }
                            break;
                        }
                        case SDLK_HOME: {
                            if (!timeline_entries[te_idx].directory) {
                                auto idx = find_prev_dir_idx(timeline_entries, te_idx);
                                if (idx + 1 < nr_te_entries)  {
                                    idx++;
                                }
                                if (idx != te_idx) {
                                    te_idx = idx;
                                    te = timeline_entries[te_idx];
                                    display_image(te, renderer, &texture, toggle_flag);
                                }
                            }
                            break;
                        }
                        case SDLK_END: {
                            if (!timeline_entries[te_idx].directory) {
                                auto idx = find_next_dir_idx(timeline_entries, te_idx);
                                if (idx == te_idx) { // last entry
                                    idx = nr_te_entries - 1;
                                }
                                else if (idx - 1 >= 0)  {
                                    idx--;
                                }
                                if (idx != te_idx) {
                                    te_idx = idx;
                                    te = timeline_entries[te_idx];
                                    display_image(te, renderer, &texture, toggle_flag);
                                }
                                break;
                            }
                        }
                    }
                }
                prev_key_code = event.key.keysym.sym;
            }

            if (event.type == SDL_KEYUP) {
                prev_key_code = 0; // reset
            }

            if (event.type == SDL_QUIT) {
                exit_signal = true;
            }
        }
    }
    SDL_DestroyTexture(texture);
    IMG_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}