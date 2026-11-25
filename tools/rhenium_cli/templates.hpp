#pragma once

#include <filesystem>
#include <string>

namespace rhenium::cli {

std::string sanitize_identifier(const std::string& name);
bool scaffold_project(
    const std::string& raw_name,
    const std::filesystem::path& project_dir,
    const std::string& default_rhenium_dir);

}
