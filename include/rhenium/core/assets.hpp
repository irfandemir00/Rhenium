#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace rhenium {

struct AssetView {
    const uint8_t* data = nullptr;
    size_t size = 0;
    std::string mime;
};

struct AssetData {
    std::vector<uint8_t> bytes;
    std::string mime;
};

std::string mime_type_for_path(std::string_view path);

void set_default_dev_ui_dir(std::string utf8_path);
std::string get_default_dev_ui_dir();

class EmbeddedAssetRegistry {
public:
    static EmbeddedAssetRegistry& instance();

    void register_asset(std::string path, const uint8_t* data, size_t size);
    std::optional<AssetView> find(std::string_view normalized_path) const;
    size_t count() const;
    void clear();

private:
    EmbeddedAssetRegistry() = default;

    struct Entry {
        const uint8_t* data = nullptr;
        size_t size = 0;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> assets_;
};

class AssetResolver {
public:
    AssetResolver(std::filesystem::path ui_dir, bool prefer_embedded);

    std::optional<AssetData> resolve(std::string_view request_path) const;

    const std::filesystem::path& ui_dir() const { return ui_dir_; }
    bool prefer_embedded() const { return prefer_embedded_; }

private:
    std::filesystem::path ui_dir_;
    bool prefer_embedded_ = false;
};

}
