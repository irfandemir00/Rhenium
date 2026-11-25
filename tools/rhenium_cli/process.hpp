#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rhenium::cli {

std::string wide_to_utf8(std::wstring_view wide);
std::wstring utf8_to_wide(std::string_view utf8);
std::string path_to_utf8(const std::filesystem::path& p);
std::filesystem::path utf8_to_path(std::string_view s);

int run_process(const std::vector<std::wstring>& args, const std::filesystem::path& cwd);
bool write_text_file(const std::filesystem::path& path, const std::string& content);
std::optional<std::filesystem::path> find_built_executable(const std::filesystem::path& build_dir);

}
