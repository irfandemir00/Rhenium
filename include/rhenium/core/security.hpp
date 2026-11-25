#pragma once

#include <string>
#include <string_view>

namespace rhenium::security {

inline constexpr std::string_view kAppOrigin = "https://rhenium.localhost";

bool is_trusted_origin(std::string_view url);
bool is_external_navigation(std::string_view url);
std::string normalize_asset_path(std::string_view request_path);
std::string default_csp();
std::string format_response_headers(std::string_view mime, std::string_view csp, bool is_html);

}
