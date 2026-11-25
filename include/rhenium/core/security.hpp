#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace rhenium::security {

inline constexpr std::string_view kAppOrigin = "https://rhenium.localhost";

bool is_trusted_origin(std::string_view url);
bool is_external_navigation(std::string_view url);
std::string normalize_asset_path(std::string_view request_path);
std::string default_csp();
std::string format_response_headers(std::string_view mime, std::string_view csp, bool is_html);

class FsScope {
public:
    void allow_all(bool enabled = true);
    void allow_directory(const std::filesystem::path& dir);
    void allow_file(const std::filesystem::path& file);
    bool is_allowed(const std::filesystem::path& target) const;

private:
    mutable std::mutex mutex_;
    bool allow_all_ = false;
    std::vector<std::filesystem::path> allowed_dirs_;
    std::vector<std::filesystem::path> allowed_files_;
};

}
