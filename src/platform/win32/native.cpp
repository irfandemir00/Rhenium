#include "rhenium/platform/native.hpp"

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <cwchar>
#include <vector>

namespace rhenium::native {

namespace {

std::vector<std::pair<std::wstring, std::wstring>> build_wide_filters(const std::vector<FileFilter>& filters) {
    std::vector<std::pair<std::wstring, std::wstring>> out;
    out.reserve(filters.size());
    for (const auto& f : filters) {
        out.emplace_back(utf8_to_wide(f.name), utf8_to_wide(f.pattern));
    }
    return out;
}

template <typename DialogInterface>
void apply_filters(DialogInterface* dlg, const std::vector<std::pair<std::wstring, std::wstring>>& storage) {
    if (storage.empty()) {
        return;
    }
    std::vector<COMDLG_FILTERSPEC> specs;
    specs.reserve(storage.size());
    for (const auto& [name, spec] : storage) {
        specs.push_back(COMDLG_FILTERSPEC{name.c_str(), spec.c_str()});
    }
    dlg->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    dlg->SetFileTypeIndex(1);
}

std::optional<std::filesystem::path> extract_dialog_result(IFileDialog* dlg) {
    IShellItem* item = nullptr;
    if (FAILED(dlg->GetResult(&item)) || !item) {
        return std::nullopt;
    }
    PWSTR raw_path = nullptr;
    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path)) && raw_path) {
        result = std::filesystem::path(raw_path);
        CoTaskMemFree(raw_path);
    }
    item->Release();
    return result;
}

}

std::wstring utf8_to_wide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(
        CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::string wide_to_utf8(std::wstring_view wide) {
    if (wide.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::optional<std::filesystem::path> open_file_dialog(
    void* owner_hwnd,
    const std::string& title,
    const std::vector<FileFilter>& filters) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
        reinterpret_cast<void**>(&dlg));
    if (FAILED(hr) || !dlg) {
        return std::nullopt;
    }

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    }
    if (!title.empty()) {
        dlg->SetTitle(utf8_to_wide(title).c_str());
    }
    auto filter_storage = build_wide_filters(filters);
    apply_filters(dlg, filter_storage);

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(owner_hwnd)))) {
        result = extract_dialog_result(dlg);
    }
    dlg->Release();
    return result;
}

std::optional<std::filesystem::path> save_file_dialog(
    void* owner_hwnd,
    const std::string& title,
    const std::string& default_name,
    const std::vector<FileFilter>& filters) {
    IFileSaveDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileSaveDialog,
        reinterpret_cast<void**>(&dlg));
    if (FAILED(hr) || !dlg) {
        return std::nullopt;
    }

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
    }
    if (!title.empty()) {
        dlg->SetTitle(utf8_to_wide(title).c_str());
    }
    if (!default_name.empty()) {
        dlg->SetFileName(utf8_to_wide(default_name).c_str());
    }
    auto filter_storage = build_wide_filters(filters);
    apply_filters(dlg, filter_storage);

    std::string default_ext;
    const size_t slash_pos = default_name.find_last_of("/\\");
    const size_t dot_pos = default_name.rfind('.');
    if (dot_pos != std::string::npos &&
        (slash_pos == std::string::npos || dot_pos > slash_pos) &&
        dot_pos + 1 < default_name.size()) {
        default_ext = default_name.substr(dot_pos + 1);
    } else if (!filters.empty()) {
        std::string_view pat = filters[0].pattern;
        const size_t semi = pat.find(';');
        if (semi != std::string_view::npos) {
            pat = pat.substr(0, semi);
        }
        if (pat.starts_with("*.")) {
            pat.remove_prefix(2);
        } else if (pat.starts_with(".")) {
            pat.remove_prefix(1);
        }
        if (!pat.empty() && pat != "*") {
            default_ext = std::string(pat);
        }
    }
    if (!default_ext.empty()) {
        dlg->SetDefaultExtension(utf8_to_wide(default_ext).c_str());
    }

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(owner_hwnd)))) {
        result = extract_dialog_result(dlg);
    }
    dlg->Release();
    return result;
}

std::optional<std::filesystem::path> open_folder_dialog(
    void* owner_hwnd,
    const std::string& title) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog,
        reinterpret_cast<void**>(&dlg));
    if (FAILED(hr) || !dlg) {
        return std::nullopt;
    }

    DWORD opts = 0;
    if (SUCCEEDED(dlg->GetOptions(&opts))) {
        dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
    }
    if (!title.empty()) {
        dlg->SetTitle(utf8_to_wide(title).c_str());
    }

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(owner_hwnd)))) {
        result = extract_dialog_result(dlg);
    }
    dlg->Release();
    return result;
}

bool clipboard_set_text(const std::string& utf8_text) {
    const std::wstring wide = utf8_to_wide(utf8_text);
    const size_t byte_count = (wide.size() + 1) * sizeof(wchar_t);

    if (!OpenClipboard(nullptr)) {
        return false;
    }
    EmptyClipboard();

    HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, byte_count);
    if (!hmem) {
        CloseClipboard();
        return false;
    }

    void* ptr = GlobalLock(hmem);
    if (!ptr) {
        GlobalFree(hmem);
        CloseClipboard();
        return false;
    }
    std::wmemcpy(static_cast<wchar_t*>(ptr), wide.c_str(), wide.size() + 1);
    GlobalUnlock(hmem);

    if (!SetClipboardData(CF_UNICODETEXT, hmem)) {
        GlobalFree(hmem);
        CloseClipboard();
        return false;
    }
    CloseClipboard();
    return true;
}

std::optional<std::string> clipboard_get_text() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return std::nullopt;
    }
    if (!OpenClipboard(nullptr)) {
        return std::nullopt;
    }

    std::optional<std::string> result;
    HANDLE hdata = GetClipboardData(CF_UNICODETEXT);
    if (hdata) {
        const auto* wide_ptr = static_cast<const wchar_t*>(GlobalLock(hdata));
        if (wide_ptr) {
            result = wide_to_utf8(wide_ptr);
            GlobalUnlock(hdata);
        }
    }
    CloseClipboard();
    return result;
}

bool message_box(
    void* owner_hwnd,
    const std::string& title,
    const std::string& message,
    MessageBoxType type) {
    UINT flags = MB_OK | MB_ICONINFORMATION;
    switch (type) {
        case MessageBoxType::Info:     flags = MB_OK | MB_ICONINFORMATION; break;
        case MessageBoxType::Warning:  flags = MB_OK | MB_ICONWARNING; break;
        case MessageBoxType::Error:    flags = MB_OK | MB_ICONERROR; break;
        case MessageBoxType::Question: flags = MB_YESNO | MB_ICONQUESTION; break;
    }
    const int res = MessageBoxW(
        static_cast<HWND>(owner_hwnd),
        utf8_to_wide(message).c_str(),
        utf8_to_wide(title).c_str(),
        flags);
    return type == MessageBoxType::Question ? (res == IDYES) : (res != 0);
}

bool open_external_url(const std::string& url) {
    if (url.empty()) {
        return false;
    }
    const std::wstring wide = utf8_to_wide(url);
    const INT_PTR rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    return rc > 32;
}

}
