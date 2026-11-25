#include "rhenium/core/config.hpp"

#include <fstream>
#include <stdexcept>

namespace rhenium {

namespace {

template <typename T>
void read_if_present(const nlohmann::json& obj, const char* key, T& target) {
    auto it = obj.find(key);
    if (it != obj.end() && !it->is_null()) {
        target = it->get<T>();
    }
}

}

Config Config::from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        throw std::runtime_error("Rhenium config JSON root must be an object");
    }

    Config cfg;
    read_if_present(j, "app_id", cfg.app_id);
    read_if_present(j, "title", cfg.title);
    read_if_present(j, "width", cfg.width);
    read_if_present(j, "height", cfg.height);
    read_if_present(j, "min_width", cfg.min_width);
    read_if_present(j, "min_height", cfg.min_height);
    read_if_present(j, "max_width", cfg.max_width);
    read_if_present(j, "max_height", cfg.max_height);
    read_if_present(j, "resizable", cfg.resizable);
    read_if_present(j, "frameless", cfg.frameless);
    read_if_present(j, "dark_titlebar", cfg.dark_titlebar);
    read_if_present(j, "devtools", cfg.devtools);
    read_if_present(j, "hot_reload", cfg.hot_reload);
    read_if_present(j, "start_path", cfg.start_path);
    read_if_present(j, "ui_dir", cfg.ui_dir);
    read_if_present(j, "csp", cfg.csp);

    if (auto win = j.find("window"); win != j.end() && win->is_object()) {
        read_if_present(*win, "title", cfg.title);
        read_if_present(*win, "width", cfg.width);
        read_if_present(*win, "height", cfg.height);
        read_if_present(*win, "min_width", cfg.min_width);
        read_if_present(*win, "min_height", cfg.min_height);
        read_if_present(*win, "max_width", cfg.max_width);
        read_if_present(*win, "max_height", cfg.max_height);
        read_if_present(*win, "resizable", cfg.resizable);
        read_if_present(*win, "frameless", cfg.frameless);
        read_if_present(*win, "dark_titlebar", cfg.dark_titlebar);
    }

    if (auto sec = j.find("security"); sec != j.end() && sec->is_object()) {
        read_if_present(*sec, "csp", cfg.csp);
        read_if_present(*sec, "devtools", cfg.devtools);
    }

    if (cfg.width < 100) cfg.width = 100;
    if (cfg.height < 100) cfg.height = 100;
    if (cfg.min_width < 0) cfg.min_width = 0;
    if (cfg.min_height < 0) cfg.min_height = 0;
    if (cfg.start_path.empty() || cfg.start_path.front() != '/') {
        cfg.start_path = "/" + cfg.start_path;
    }

    return cfg;
}

Config Config::from_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Config{};
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Failed to open Rhenium config file");
    }

    nlohmann::json parsed;
    try {
        ifs >> parsed;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Invalid JSON in Rhenium config file: ") + e.what());
    }
    return Config::from_json(parsed);
}

}
