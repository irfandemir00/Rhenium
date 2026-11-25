#include "process.hpp"

#include <windows.h>

#include <fstream>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace rhenium::cli {

namespace {

std::wstring quote_arg(const std::wstring& arg) {
    if (arg.find_first_of(L" \t\"") == std::wstring::npos && !arg.empty()) {
        return arg;
    }
    std::wstring out = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') {
            out += L"\\\"";
        } else {
            out.push_back(c);
        }
    }
    out += L"\"";
    return out;
}

}

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int len = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), len, nullptr, nullptr);
    return out;
}

std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), len);
    return out;
}

std::string path_to_utf8(const fs::path& p) {
    return wide_to_utf8(p.wstring());
}

fs::path utf8_to_path(std::string_view s) {
    return fs::path(utf8_to_wide(s));
}

int run_process(const std::vector<std::wstring>& args, const fs::path& cwd) {
    std::wstring cmdline;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmdline += L' ';
        cmdline += quote_arg(args[i]);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> mutable_cmd(cmdline.begin(), cmdline.end());
    mutable_cmd.push_back(L'\0');

    const std::wstring cwd_w = cwd.wstring();
    const BOOL ok = CreateProcessW(
        nullptr,
        mutable_cmd.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        cwd_w.empty() ? nullptr : cwd_w.c_str(),
        &si,
        &pi);
    if (!ok) {
        std::cerr << "Failed to launch process (Win32 error " << GetLastError() << ")\n";
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exit_code);
}

bool write_text_file(const fs::path& path, const std::string& content) {
    std::error_code ec;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    return ofs.good();
}

std::optional<fs::path> find_built_executable(const fs::path& build_dir) {
    std::error_code ec;
    if (!fs::exists(build_dir, ec)) return std::nullopt;

    for (const auto& entry : fs::directory_iterator(build_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const auto& p = entry.path();
        if (p.extension() == L".exe") {
            const std::string name = path_to_utf8(p.filename());
            if (name != "rhenium_pack.exe" && name != "rhenium.exe") {
                return p;
            }
        }
    }
    return std::nullopt;
}

}
