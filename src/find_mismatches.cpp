#include "fs_scanner.hpp"

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include <string>
#include <mhash.h>
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
#include <stdlib.h>

bool file_exists(std::string path) {
    struct stat sb;
	bool found;
	int result;

    result = stat(path.c_str(), &sb);
    found = (result == 0);

    return found;
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

void print_missing_output_files(std::string root_path) {
    
    printf("Expected files that are missing:\n");

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
                //printf("Scanning '%s'\n", level.c_str());

                auto level_sha1_base = "/home/output/" + string_to_sha1(level);
                auto dir_raw_path = level_sha1_base + "_1080.jpg";
                auto dir_tagged_path = level_sha1_base + "_1080_tagged.jpg";

                if (!file_exists(dir_raw_path)) {
                    printf("%s ('%s')\n", dir_raw_path.c_str(), level.c_str());
                }

                if (!file_exists(dir_tagged_path)) {
                    printf("%s ('%s')\n", dir_tagged_path.c_str(), level.c_str());
                }

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

                        if (!file_exists(img_raw_path)) {
                            printf("%s ('%s')\n", img_raw_path.c_str(), hash_path.c_str());
                        }

                        if (!file_exists(img_tagged_path)) {
                            printf("%s ('%s')\n", img_tagged_path.c_str(), hash_path.c_str());
                        }
                    }
                }
            }
            
            found_dirs[level] = true;
        }
    }
}

int main()
{

    std::string root_path = "/home/lonezor/Media/media_timeline";

    print_missing_output_files(root_path);

    return 0;
}