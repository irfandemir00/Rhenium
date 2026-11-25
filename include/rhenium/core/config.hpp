#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace rhenium {

struct Config {
    std::string app_id = "com.rhenium.app";
    std::string title = "Rhenium App";
    int width = 1024;
    int height = 680;
    int min_width = 400;
    int min_height = 300;
    int max_width = 0;
    int max_height = 0;
    bool resizable = true;
    bool frameless = false;
    bool dark_titlebar = true;
#ifdef NDEBUG
    bool devtools = false;
    bool hot_reload = false;
#else
    bool devtools = true;
    bool hot_reload = true;
#endif
    std::string start_path = "/index.html";
    std::string ui_dir = "ui";
    std::string csp;

    static Config from_json(const nlohmann::json& j);
    static Config from_file(const std::filesystem::path& path);
};

}
