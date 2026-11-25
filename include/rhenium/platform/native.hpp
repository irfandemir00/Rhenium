#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rhenium::native {

struct FileFilter {
    std::string name;
    std::string pattern;
};

enum class MessageBoxType {
    Info,
    Warning,
    Error,
    Question
};

std::wstring utf8_to_wide(std::string_view utf8);
std::string wide_to_utf8(std::wstring_view wide);

std::optional<std::filesystem::path> open_file_dialog(
    void* owner_hwnd,
    const std::string& title = "Open File",
    const std::vector<FileFilter>& filters = {});

std::optional<std::filesystem::path> save_file_dialog(
    void* owner_hwnd,
    const std::string& title = "Save File",
    const std::string& default_name = "",
    const std::vector<FileFilter>& filters = {});

std::optional<std::filesystem::path> open_folder_dialog(
    void* owner_hwnd,
    const std::string& title = "Select Folder");

bool clipboard_set_text(const std::string& utf8_text);
std::optional<std::string> clipboard_get_text();

bool message_box(
    void* owner_hwnd,
    const std::string& title,
    const std::string& message,
    MessageBoxType type = MessageBoxType::Info);

bool open_external_url(const std::string& url);

}
