#include <initguid.h>
#include "webview2_loader.hpp"

#include <filesystem>

namespace rhenium::detail {

CreateWebView2EnvFn load_webview2_create_env() {
    static HMODULE loader_module = nullptr;
    static CreateWebView2EnvFn create_fn = nullptr;

    if (create_fn != nullptr) {
        return create_fn;
    }

    wchar_t exe_path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path beside_exe = std::filesystem::path(exe_path).parent_path() / L"WebView2Loader.dll";
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
