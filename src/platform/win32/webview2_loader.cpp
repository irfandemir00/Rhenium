#include <initguid.h>
#include "webview2_loader.hpp"

#include <filesystem>
#include <string>

namespace rhenium::detail {

namespace {

std::filesystem::path get_current_exe_dir() {
    std::wstring buf(MAX_PATH, L'\0');
    while (true) {
        const DWORD len = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (len == 0) {
            return {};
        }
        if (len < buf.size() - 1) {
            buf.resize(len);
            return std::filesystem::path(buf).parent_path();
        }
        if (buf.size() >= 32768) {
            return {};
        }
        buf.resize(buf.size() * 2);
    }
}

}

CreateWebView2EnvFn load_webview2_create_env() {
    static HMODULE loader_module = nullptr;
    static CreateWebView2EnvFn create_fn = nullptr;

    if (create_fn != nullptr) {
        return create_fn;
    }

    const auto exe_dir = get_current_exe_dir();
    if (!exe_dir.empty()) {
        std::filesystem::path beside_exe = exe_dir / L"WebView2Loader.dll";
        loader_module = LoadLibraryW(beside_exe.c_str());
    }

    if (!loader_module) {
        loader_module = LoadLibraryW(L"WebView2Loader.dll");
    }

    if (!loader_module) {
        return nullptr;
    }

    create_fn = reinterpret_cast<CreateWebView2EnvFn>(
        GetProcAddress(loader_module, "CreateCoreWebView2EnvironmentWithOptions"));
    return create_fn;
}

}
