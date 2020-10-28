#include "fs_scanner.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <cstring>

enum cli_option
{
    cli_option_timestamp = 1000,
    cli_option_title,
    cli_option_location,
    cli_option_description,
    cli_option_black,
    cli_option_white,
    cli_option_help,
};

static const char* g_timestamp = nullptr;
static const char* g_title = nullptr;
static const char* g_location = nullptr;
static const char* g_description = nullptr;
static bool g_black = false;
static bool g_white = true; // default since it works best for the majority of images
static bool g_help = false;

static struct option long_options[] = { { "timestamp", required_argument, 0, cli_option_timestamp },
                                        { "title", required_argument, 0, cli_option_title },
                                        { "location", required_argument, 0, cli_option_location },
                                        { "description", required_argument, 0, cli_option_description },
                                        { "black", no_argument, 0, cli_option_black },
                                        { "white", no_argument, 0, cli_option_white },
                                        { "help", no_argument, 0, cli_option_help },
                                        { 0, 0, 0, 0 } };

static void
print_help()
{
    std::cout << "Usage: update_info [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "This tool updates the existing .info files. It maintains the hash value and sets the other attributes" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << " --timestamp=STRING     Custom timestamp string" << std::endl;
    std::cout << " --title=STRING         Custom title string" << std::endl;
    std::cout << " --location=STRING      Custom location string" << std::endl;
    std::cout << " --description=STRING   Custom location string" << std::endl;
    std::cout << " --black                Black text color for all attributes" << std::endl;
    std::cout << " --white                White text color for all attributes (default)" << std::endl;
    std::cout << " -h --help              This help screen" << std::endl;
    std::cout << std::endl;
}

static bool
parseArguments(int argc, char* argv[])
{
    // Parse command line arguments
    int c;
    int option_index = 0;
    while (true) {
        c = getopt_long(argc, argv, "h", long_options, &option_index);

        // All options parsed
        if (c == -1) {
            break;
        }

        switch (c) // Listed in order of importance
        {
            case cli_option_timestamp:
                g_timestamp = optarg;
                break;

            case cli_option_title:
                g_title = optarg;
                break;

            case cli_option_location:
                g_location = optarg;
                break;

            case cli_option_description:
                g_description = optarg;
                break;

            case cli_option_black:
                g_black = true;
                g_white = false;
                break;

            case cli_option_white:
                g_black = false;
                g_white = true;
                break;

            case 'h':
            case cli_option_help:
                g_help = true;
                break;

            default:
                continue;
        }
    }

    if (g_help) {
        return true;
    }

    return true;
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

void update_info_file(std::string info_path)
{
    std::string sha1;

    // Read sha sum
    std::stringstream input;

    try {
        std::ifstream ifs(info_path);
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

    if (sha1.empty()) {
        printf("info file doesn't have sha sum, skipping update\n");
        return;
    }

    // Prepare new file contents
    std::string info = "sha1: " + sha1 + "\n";

    std::string prefix;
    if (g_black) {
        prefix = "B_";
    } else {
        prefix = "W_";
    }

    if (g_timestamp != nullptr) {
        info += prefix + "Time: " + std::string(g_timestamp) + "\n";
    }

    if (g_title != nullptr) {
        info += prefix + "Title: " + std::string(g_title) + "\n";
    }

    if (g_location != nullptr) {
        info += prefix + "Location: " + std::string(g_location) + "\n";
    }

    // Needs to be last since it may be multi line
    if (g_description != nullptr) {
        info += prefix + "MediaDescription: " + std::string(g_description) + "\n";
    }

    printf("Updating '%s'\n"
           "==================================================================\n"
           "%s\n", info_path.c_str(), info.c_str());
    
    std::ofstream out(info_path);
    out << info;
    out.close();
}

int main(int argc, char* argv[])
{
    // Parse mandatory and optional arguments
    if (!parseArguments(argc, argv)) {
        return EXIT_FAILURE;
    }

    // Print Help Screen and exit
    if (g_help) {
        print_help();
        return EXIT_SUCCESS;
    }

    char cwd[LINE_MAX];
    memset(cwd, 0, sizeof(cwd));
    getcwd(cwd, sizeof(cwd));

    auto scanner = fs_scanner();
    scanner.set_recursive(false);
    scanner.scan(std::string(cwd), ".*info");
    auto path_map = scanner.path_map();

    if (path_map.size() == 0) {
        printf("No info file found\n");
        return 0;
    }

    for(auto&& level_it : path_map) {
            auto dir = level_it.first;
            auto paths = path_map[dir];
        for(auto info_path : paths) {
            update_info_file(info_path);
        }
    }





    return 0;
}