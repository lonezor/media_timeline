#pragma once

#include <string>
#include <vector>
#include <map>

using dir_name_t = std::string;

class fs_scanner
{
    public:
        void scan(std::string path, std::string filter_regex);

        void set_recursive(bool recursive);

        void clear() { path_map_.clear(); }

        std::map<dir_name_t, std::vector<std::string>> path_map() { return path_map_; }

    private:
        std::map<dir_name_t, std::vector<std::string>> path_map_;
        bool recursive_{true};
};

