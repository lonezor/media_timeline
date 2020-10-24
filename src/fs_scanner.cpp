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

#include "fs_scanner.hpp"

void fs_scanner::set_recursive(bool recursive)
{
	recursive_ = recursive;
}

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
		else if (type == DT_DIR && recursive_) {
			scan(path, filter_regex);
		}
		free(namelist[i]);
	}
	free(namelist);
}

