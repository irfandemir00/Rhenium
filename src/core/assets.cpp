#include "rhenium/core/assets.hpp"
#include "rhenium/core/security.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_map>

namespace rhenium {

namespace {

std::string to_lower_ext(std::string_view path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string ext(path.substr(dot));
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext;
}

std::mutex& dev_ui_dir_mutex() {
    static std::mutex m;
    return m;
}

std::string& dev_ui_dir_storage() {
    static std::string dir;
    return dir;
}

}

void set_default_dev_ui_dir(std::string utf8_path) {
    std::lock_guard<std::mutex> lock(dev_ui_dir_mutex());
    dev_ui_dir_storage() = std::move(utf8_path);
}

std::string get_default_dev_ui_dir() {
    std::lock_guard<std::mutex> lock(dev_ui_dir_mutex());
    return dev_ui_dir_storage();
}

std::string mime_type_for_path(std::string_view path) {
    static const std::unordered_map<std::string, std::string> kMimeTypes = {
        {".html", "text/html; charset=utf-8"},
        {".htm", "text/html; charset=utf-8"},
        {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript; charset=utf-8"},
        {".mjs", "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".svg", "image/svg+xml"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".webp", "image/webp"},
        {".ico", "image/x-icon"},
        {".woff", "font/woff"},
        {".woff2", "font/woff2"},
        {".ttf", "font/ttf"},
        {".otf", "font/otf"},
        {".txt", "text/plain; charset=utf-8"},
        {".xml", "application/xml; charset=utf-8"},
    };

    const std::string ext = to_lower_ext(path);
    if (auto it = kMimeTypes.find(ext); it != kMimeTypes.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

EmbeddedAssetRegistry& EmbeddedAssetRegistry::instance() {
    static EmbeddedAssetRegistry registry;
    return registry;
}

void EmbeddedAssetRegistry::register_asset(std::string path, const uint8_t* data, size_t size) {
    std::string normalized = security::normalize_asset_path(path);
    if (normalized.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    assets_.insert_or_assign(std::move(normalized), Entry{data, size});
}

std::optional<AssetView> EmbeddedAssetRegistry::find(std::string_view normalized_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = assets_.find(std::string(normalized_path));
    if (it == assets_.end()) {
        return std::nullopt;
    }
    return AssetView{it->second.data, it->second.size, mime_type_for_path(normalized_path)};
}

size_t EmbeddedAssetRegistry::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return assets_.size();
}

void EmbeddedAssetRegistry::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    assets_.clear();
}

AssetResolver::AssetResolver(std::filesystem::path ui_dir, bool prefer_embedded)
    : ui_dir_(std::move(ui_dir)), prefer_embedded_(prefer_embedded) {}

std::optional<AssetData> AssetResolver::resolve(std::string_view request_path) const {
    const std::string normalized = security::normalize_asset_path(request_path);
    if (normalized.empty()) {
        return std::nullopt;
    }

    if (prefer_embedded_) {
        if (auto view = EmbeddedAssetRegistry::instance().find(normalized); view.has_value()) {
            AssetData data;
            data.mime = view->mime;
            if (view->data != nullptr && view->size > 0) {
                data.bytes.assign(view->data, view->data + view->size);
            }
            return data;
        }
    }

    if (ui_dir_.empty()) {
        return std::nullopt;
    }

    std::error_code ec;
    const auto base = std::filesystem::weakly_canonical(ui_dir_, ec);
    if (ec || !std::filesystem::exists(base, ec)) {
        return std::nullopt;
    }

    const std::string rel_utf8 = normalized.substr(1);
#ifdef _WIN32
    std::filesystem::path rel;
    if (!rel_utf8.empty()) {
        const int wlen = MultiByteToWideChar(
            CP_UTF8, 0, rel_utf8.data(), static_cast<int>(rel_utf8.size()), nullptr, 0);
        if (wlen > 0) {
            std::wstring wrel(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, rel_utf8.data(), static_cast<int>(rel_utf8.size()), wrel.data(), wlen);
            rel = std::filesystem::path(wrel);
        }
    }
#else
    const std::filesystem::path rel(rel_utf8);
#endif
    const auto candidate = std::filesystem::weakly_canonical(base / rel, ec);
    if (ec || !std::filesystem::is_regular_file(candidate, ec)) {
        return std::nullopt;
    }

    auto base_it = base.begin();
    auto cand_it = candidate.begin();
    for (; base_it != base.end(); ++base_it, ++cand_it) {
        if (cand_it == candidate.end() || *base_it != *cand_it) {
            return std::nullopt;
        }
    }

    std::ifstream ifs(candidate, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        return std::nullopt;
    }

    const std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    AssetData data;
    data.mime = mime_type_for_path(normalized);
    if (size > 0) {
        data.bytes.resize(static_cast<size_t>(size));
        if (!ifs.read(reinterpret_cast<char*>(data.bytes.data()), size)) {
            return std::nullopt;
        }
    }
    return data;
}

}
