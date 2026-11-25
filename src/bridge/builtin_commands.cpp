#include "bridge/builtin_commands.hpp"

#include "rhenium/core/security.hpp"
#include "rhenium/platform/native.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifndef RHENIUM_VERSION
#define RHENIUM_VERSION "0.1.0"
#endif

namespace rhenium::detail {

namespace {

std::vector<native::FileFilter> parse_file_filters(const nlohmann::json& opts) {
    std::vector<native::FileFilter> filters;
    if (!opts.is_object() || !opts.contains("filters") || !opts["filters"].is_array()) {
        return filters;
    }
    for (const auto& item : opts["filters"]) {
        if (!item.is_object()) continue;
        native::FileFilter f;
        f.name = item.value("name", "Files");
        f.pattern = item.value("pattern", "*.*");
        filters.push_back(std::move(f));
    }
    return filters;
}

native::MessageBoxType parse_msgbox_type(std::string_view type) {
    if (type == "warning") return native::MessageBoxType::Warning;
    if (type == "error") return native::MessageBoxType::Error;
    if (type == "question") return native::MessageBoxType::Question;
    return native::MessageBoxType::Info;
}

}

void register_builtin_commands(App& app) {
    app.command("rh:window:minimize", [&app]() { app.window().minimize(); });
    app.command("rh:window:toggleMaximize", [&app]() -> bool {
        app.window().toggle_maximize();
        return app.window().is_maximized();
    });
    app.command("rh:window:isMaximized", [&app]() -> bool { return app.window().is_maximized(); });
    app.command("rh:window:close", [&app]() { app.window().close(); });
    app.command("rh:window:setTitle", [&app](std::string title) { app.window().set_title(title); });
    app.command("rh:window:startDrag", [&app]() { app.window().start_drag(); });
    app.command("rh:window:openDevTools", [&app]() { app.window().open_devtools(); });
    app.command("rh:window:reload", [&app]() { app.window().reload(); });

    app.command("rh:dialog:openFile", [&app](nlohmann::json opts) -> nlohmann::json {
        const std::string title = opts.is_object() ? opts.value("title", "Open File") : "Open File";
        auto picked = native::open_file_dialog(app.window().native_handle(), title, parse_file_filters(opts));
        if (picked.has_value()) {
            app.fs_scope().allow_file(*picked);
            return nlohmann::json(native::wide_to_utf8(picked->wstring()));
        }
        return nlohmann::json(nullptr);
    });

    app.command("rh:dialog:saveFile", [&app](nlohmann::json opts) -> nlohmann::json {
        const std::string title = opts.is_object() ? opts.value("title", "Save File") : "Save File";
        const std::string default_name = opts.is_object() ? opts.value("defaultName", "") : "";
        auto picked = native::save_file_dialog(
            app.window().native_handle(), title, default_name, parse_file_filters(opts));
        if (picked.has_value()) {
            app.fs_scope().allow_file(*picked);
            return nlohmann::json(native::wide_to_utf8(picked->wstring()));
        }
        return nlohmann::json(nullptr);
    });

    app.command("rh:dialog:openFolder", [&app](nlohmann::json opts) -> nlohmann::json {
        const std::string title = opts.is_object() ? opts.value("title", "Select Folder") : "Select Folder";
        auto picked = native::open_folder_dialog(app.window().native_handle(), title);
        if (picked.has_value()) {
            app.fs_scope().allow_directory(*picked);
            return nlohmann::json(native::wide_to_utf8(picked->wstring()));
        }
        return nlohmann::json(nullptr);
    });

    app.command("rh:dialog:messageBox", [&app](nlohmann::json opts) -> bool {
        const std::string title = opts.is_object() ? opts.value("title", app.config().title) : app.config().title;
        const std::string message = opts.is_object() ? opts.value("message", "") : "";
        const std::string type = opts.is_object() ? opts.value("type", "info") : "info";
        return native::message_box(app.window().native_handle(), title, message, parse_msgbox_type(type));
    });

    app.command("rh:clipboard:readText", []() -> nlohmann::json {
        auto text = native::clipboard_get_text();
        return text.has_value() ? nlohmann::json(*text) : nlohmann::json(nullptr);
    });

    app.command("rh:clipboard:writeText", [](std::string text) -> bool {
        return native::clipboard_set_text(text);
    });

    app.async_command("rh:fs:readText", [&app](std::string path) -> std::string {
        const std::filesystem::path fs_path(native::utf8_to_wide(path));
        if (!app.fs_scope().is_allowed(fs_path)) {
            throw std::runtime_error("Filesystem access denied: " + path);
        }
        std::error_code ec;
        const size_t max_bytes = app.config().max_read_file_bytes;
        if (max_bytes > 0) {
            const auto sz = std::filesystem::file_size(fs_path, ec);
            if (!ec && sz > max_bytes) {
                throw std::runtime_error("File exceeds maximum allowed read size");
            }
        }
        std::ifstream ifs(fs_path, std::ios::binary);
        if (!ifs.is_open()) {
            throw std::runtime_error("Cannot open file for reading: " + path);
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        return ss.str();
    });

    app.async_command("rh:fs:writeText", [&app](std::string path, std::string content) -> bool {
        const std::filesystem::path fs_path(native::utf8_to_wide(path));
        if (!app.fs_scope().is_allowed(fs_path)) {
            throw std::runtime_error("Filesystem access denied: " + path);
        }
        std::error_code ec;
        if (fs_path.has_parent_path()) {
            std::filesystem::create_directories(fs_path.parent_path(), ec);
        }
        std::ofstream ofs(fs_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            throw std::runtime_error("Cannot open file for writing: " + path);
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!ofs.good()) {
            throw std::runtime_error("Failed writing file: " + path);
        }
        return true;
    });

    app.async_command("rh:fs:exists", [&app](std::string path) -> bool {
        const std::filesystem::path fs_path(native::utf8_to_wide(path));
        if (!app.fs_scope().is_allowed(fs_path)) {
            throw std::runtime_error("Filesystem access denied: " + path);
        }
        std::error_code ec;
        return std::filesystem::exists(fs_path, ec);
    });

    app.async_command("rh:fs:stat", [&app](std::string path) -> nlohmann::json {
        const std::filesystem::path fs_path(native::utf8_to_wide(path));
        if (!app.fs_scope().is_allowed(fs_path)) {
            throw std::runtime_error("Filesystem access denied: " + path);
        }
        std::error_code ec;
        const bool exists = std::filesystem::exists(fs_path, ec);
        const bool is_file = exists && std::filesystem::is_regular_file(fs_path, ec);
        const bool is_dir = exists && std::filesystem::is_directory(fs_path, ec);
        uintmax_t size = 0;
        if (is_file) {
            size = std::filesystem::file_size(fs_path, ec);
            if (ec) size = 0;
        }
        return nlohmann::json{
            {"exists", exists},
            {"isFile", is_file},
            {"isDirectory", is_dir},
            {"size", size}
        };
    });

    app.async_command("rh:fs:listDir", [&app](std::string path) -> nlohmann::json {
        const std::filesystem::path fs_path(native::utf8_to_wide(path));
        if (!app.fs_scope().is_allowed(fs_path)) {
            throw std::runtime_error("Filesystem access denied: " + path);
        }
        std::error_code ec;
        if (!std::filesystem::exists(fs_path, ec) || !std::filesystem::is_directory(fs_path, ec)) {
            throw std::runtime_error("Directory does not exist: " + path);
        }
        struct Item {
            std::string name;
            std::string full_path;
            bool is_dir = false;
            uintmax_t size = 0;
        };
        std::vector<Item> items;
        constexpr size_t kMaxEntries = 1000;
        for (const auto& entry : std::filesystem::directory_iterator(
                 fs_path, std::filesystem::directory_options::skip_permission_denied, ec)) {
            if (items.size() >= kMaxEntries) break;
            std::error_code item_ec;
            const bool is_dir = entry.is_directory(item_ec);
            uintmax_t sz = 0;
            if (!is_dir && entry.is_regular_file(item_ec)) {
                sz = entry.file_size(item_ec);
                if (item_ec) sz = 0;
            }
            items.push_back(Item{
                native::wide_to_utf8(entry.path().filename().wstring()),
                native::wide_to_utf8(entry.path().wstring()),
                is_dir,
                sz
            });
        }
        std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
            if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
            return a.name < b.name;
        });
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& it : items) {
            arr.push_back(nlohmann::json{
                {"name", it.name},
                {"path", it.full_path},
                {"isDirectory", it.is_dir},
                {"size", it.size}
            });
        }
        return arr;
    });

    app.command("rh:shell:openExternal", [](std::string url) -> bool {
        if (!security::is_external_navigation(url)) {
            throw std::invalid_argument("Only external http/https/mailto URLs are allowed");
        }
        return native::open_external_url(url);
    });

    app.command("rh:app:getInfo", [&app]() -> nlohmann::json {
        return nlohmann::json{
            {"appId", app.config().app_id},
            {"title", app.config().title},
            {"embedded", app.resolver().prefer_embedded()},
            {"devtools", app.config().devtools},
            {"platform", "windows"},
            {"version", RHENIUM_VERSION}
        };
    });
}

}
