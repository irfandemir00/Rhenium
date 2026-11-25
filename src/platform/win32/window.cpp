#include "rhenium/platform/window.hpp"

#include "rhenium/bridge/runtime_js.hpp"
#include "rhenium/core/security.hpp"
#include "rhenium/platform/native.hpp"
#include "webview2_loader.hpp"

#include <windows.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <windowsx.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace rhenium {

namespace {

constexpr UINT WM_RHENIUM_DISPATCH = WM_APP + 101;
constexpr wchar_t kWindowClassName[] = L"RheniumHostWindowClass";

void enable_per_monitor_dpi() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    using SetDpiCtxFn = BOOL(WINAPI*)(HANDLE);
    auto fn = reinterpret_cast<SetDpiCtxFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
    if (fn) {
        fn(reinterpret_cast<HANDLE>(-4));
    }
}

UINT get_system_dpi_safe() {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForSystemFn = UINT(WINAPI*)();
        auto fn = reinterpret_cast<GetDpiForSystemFn>(GetProcAddress(user32, "GetDpiForSystem"));
        if (fn) {
            const UINT dpi = fn();
            if (dpi > 0) return dpi;
        }
    }
    return 96;
}

UINT get_window_dpi_safe(HWND hwnd) {
    if (hwnd) {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32) {
            using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
            auto fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
            if (fn) {
                const UINT dpi = fn(hwnd);
                if (dpi > 0) return dpi;
            }
        }
    }
    return get_system_dpi_safe();
}

void adjust_window_rect_for_dpi(RECT* rect, DWORD style, BOOL menu, DWORD ex_style, UINT dpi) {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using AdjustForDpiFn = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        auto fn = reinterpret_cast<AdjustForDpiFn>(GetProcAddress(user32, "AdjustWindowRectExForDpi"));
        if (fn && fn(rect, style, menu, ex_style, dpi)) {
            return;
        }
    }
    AdjustWindowRectEx(rect, style, menu, ex_style);
}

void apply_dark_titlebar(HWND hwnd, bool enabled) {
    const BOOL dark = enabled ? TRUE : FALSE;
    constexpr DWORD kDwmUseImmersiveDarkMode = 20;
    constexpr DWORD kDwmUseImmersiveDarkModeWin10 = 19;
    if (FAILED(DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkMode, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkModeWin10, &dark, sizeof(dark));
    }
}

std::filesystem::path build_user_data_dir(std::string_view app_id) {
    wchar_t* local_app_data = nullptr;
    size_t len = 0;
    std::filesystem::path base;
    if (_wdupenv_s(&local_app_data, &len, L"LOCALAPPDATA") == 0 && local_app_data) {
        base = std::filesystem::path(local_app_data);
        free(local_app_data);
    } else {
        base = std::filesystem::temp_directory_path();
    }
    std::string safe_id(app_id.empty() ? "com.rhenium.app" : app_id);
    for (char& c : safe_id) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return base / L"Rhenium" / native::utf8_to_wide(safe_id);
}

}

struct Window::Impl : public std::enable_shared_from_this<Window::Impl> {
    Config config;
    const AssetResolver& resolver;
    std::atomic<HWND> hwnd{nullptr};

    detail::ComPtr<ICoreWebView2Environment> env;
    detail::ComPtr<ICoreWebView2Controller> controller;
    detail::ComPtr<ICoreWebView2> webview;

    MessageCallback on_message;
    ReadyCallback on_ready;
    DispatchCallback on_dispatch;

    std::atomic<bool> ready{false};
    std::atomic<bool> destroyed{false};
    std::atomic<bool> on_ready_fired{false};
    bool last_maximized = false;
    std::mutex pending_mutex;
    std::vector<nlohmann::json> pending_messages;

    Impl(const Config& cfg, const AssetResolver& res) : config(cfg), resolver(res) {}

    void resize_webview() {
        HWND h = hwnd.load();
        if (!controller || !h) return;
        RECT bounds{};
        GetClientRect(h, &bounds);
        controller->put_Bounds(bounds);
    }

    void flush_pending_messages() {
        std::vector<nlohmann::json> queued;
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            queued.swap(pending_messages);
        }
        for (const auto& msg : queued) {
            post_json(msg);
        }
    }

    void post_json(const nlohmann::json& msg) {
        if (destroyed.load()) {
            return;
        }
        if (!ready.load() || !webview) {
            std::lock_guard<std::mutex> lock(pending_mutex);
            pending_messages.push_back(msg);
            return;
        }
        const std::wstring wide = native::utf8_to_wide(msg.dump());
        webview->PostWebMessageAsJson(wide.c_str());
    }

    void emit_event(std::string_view event, const nlohmann::json& payload) {
        post_json(nlohmann::json{
            {"kind", "event"},
            {"name", event},
            {"payload", payload}
        });
    }

    void init_webview2() {
        HWND h = hwnd.load();
        auto create_env = detail::load_webview2_create_env();
        if (!create_env) {
            native::message_box(
                h,
                "Rhenium — WebView2 Error",
                "Could not load WebView2Loader.dll or Microsoft Edge WebView2 Runtime.",
                native::MessageBoxType::Error);
            if (h) DestroyWindow(h);
            return;
        }

        const std::filesystem::path user_data = build_user_data_dir(config.app_id);
        std::error_code ec;
        std::filesystem::create_directories(user_data, ec);

        std::weak_ptr<Impl> weak = weak_from_this();
        auto* env_handler = detail::make_com_handler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
            [weak](HRESULT hr, ICoreWebView2Environment* created_env) -> HRESULT {
                auto self = weak.lock();
                if (!self || self->destroyed.load()) {
                    return S_OK;
                }
                HWND cur_hwnd = self->hwnd.load();
                if (FAILED(hr) || !created_env || !cur_hwnd) {
                    native::message_box(
                        cur_hwnd,
                        "Rhenium — WebView2 Error",
                        "Failed to create WebView2 environment. Ensure Microsoft Edge WebView2 Runtime is installed.",
                        native::MessageBoxType::Error);
                    if (cur_hwnd) DestroyWindow(cur_hwnd);
                    return hr;
                }
                created_env->AddRef();
                self->env.attach(created_env);

                auto* ctrl_handler = detail::make_com_handler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                    [weak](HRESULT ctrl_hr, ICoreWebView2Controller* created_ctrl) -> HRESULT {
                        auto self_ctrl = weak.lock();
                        if (!self_ctrl || self_ctrl->destroyed.load()) {
                            return S_OK;
                        }
                        HWND h_ctrl = self_ctrl->hwnd.load();
                        if (FAILED(ctrl_hr) || !created_ctrl || !h_ctrl) {
                            native::message_box(
                                h_ctrl,
                                "Rhenium — WebView2 Error",
                                "Failed to create WebView2 controller.",
                                native::MessageBoxType::Error);
                            if (h_ctrl) DestroyWindow(h_ctrl);
                            return FAILED(ctrl_hr) ? ctrl_hr : E_FAIL;
                        }
                        created_ctrl->AddRef();
                        self_ctrl->controller.attach(created_ctrl);

                        ICoreWebView2* raw_wv = nullptr;
                        if (FAILED(self_ctrl->controller->get_CoreWebView2(&raw_wv)) || !raw_wv) {
                            native::message_box(
                                h_ctrl,
                                "Rhenium — WebView2 Error",
                                "Failed to obtain CoreWebView2 instance.",
                                native::MessageBoxType::Error);
                            DestroyWindow(h_ctrl);
                            return E_FAIL;
                        }
                        self_ctrl->webview.attach(raw_wv);

                        self_ctrl->configure_webview();
                        self_ctrl->resize_webview();

                        const std::string entry_url = std::string(security::kAppOrigin) + self_ctrl->config.start_path;
                        self_ctrl->webview->Navigate(native::utf8_to_wide(entry_url).c_str());
                        return S_OK;
                    });

                const HRESULT c_hr = self->env->CreateCoreWebView2Controller(cur_hwnd, ctrl_handler);
                ctrl_handler->Release();
                if (FAILED(c_hr)) {
                    native::message_box(
                        cur_hwnd,
                        "Rhenium — WebView2 Error",
                        "CreateCoreWebView2Controller failed.",
                        native::MessageBoxType::Error);
                    DestroyWindow(cur_hwnd);
                }
                return c_hr;
            });

        const HRESULT e_hr = create_env(nullptr, user_data.c_str(), nullptr, env_handler);
        env_handler->Release();
        if (FAILED(e_hr)) {
            native::message_box(
                h,
                "Rhenium — WebView2 Error",
                "CreateCoreWebView2EnvironmentWithOptions failed.",
                native::MessageBoxType::Error);
            if (h) DestroyWindow(h);
        }
    }

    void configure_webview() {
        detail::ComPtr<ICoreWebView2Settings> settings;
        if (SUCCEEDED(webview->get_Settings(settings.put())) && settings) {
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_AreDevToolsEnabled(config.devtools ? TRUE : FALSE);
            settings->put_AreDefaultContextMenusEnabled(config.devtools ? TRUE : FALSE);
            settings->put_IsWebMessageEnabled(TRUE);

            detail::ComPtr<ICoreWebView2Settings3> settings3;
            if (SUCCEEDED(settings->QueryInterface(IID_ICoreWebView2Settings3, reinterpret_cast<void**>(settings3.put()))) && settings3) {
                settings3->put_AreBrowserAcceleratorKeysEnabled(config.devtools ? TRUE : FALSE);
            }
        }

        const std::wstring script_w = native::utf8_to_wide(
            build_runtime_script(config.frameless, config.resizable));
        webview->AddScriptToExecuteOnDocumentCreated(script_w.c_str(), nullptr);

        webview->AddWebResourceRequestedFilter(
            L"https://rhenium.localhost/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

        EventRegistrationToken token{};
        std::weak_ptr<Impl> weak = weak_from_this();

        auto* res_handler = detail::make_com_handler<ICoreWebView2WebResourceRequestedEventHandler>(
            IID_ICoreWebView2WebResourceRequestedEventHandler,
            [weak](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                auto self = weak.lock();
                if (!self || self->destroyed.load() || !self->env) {
                    return S_OK;
                }
                detail::ComPtr<ICoreWebView2WebResourceRequest> req;
                if (FAILED(args->get_Request(req.put())) || !req) {
                    return S_OK;
                }
                LPWSTR raw_uri = nullptr;
                if (FAILED(req->get_Uri(&raw_uri)) || !raw_uri) {
                    return S_OK;
                }
                const std::string uri = native::wide_to_utf8(raw_uri);
                CoTaskMemFree(raw_uri);

                if (!security::is_trusted_origin(uri)) {
                    return S_OK;
                }

                auto asset = self->resolver.resolve(uri);
                detail::ComPtr<ICoreWebView2WebResourceResponse> response;
                if (!asset.has_value()) {
                    const char* not_found = "404 Not Found";
                    IStream* stream = SHCreateMemStream(
                        reinterpret_cast<const BYTE*>(not_found), static_cast<UINT>(strlen(not_found)));
                    self->env->CreateWebResourceResponse(
                        stream, 404, L"Not Found", L"Content-Type: text/plain; charset=utf-8\r\n",
                        response.put());
                    if (stream) stream->Release();
                    if (response) args->put_Response(response.get());
                    return S_OK;
                }

                IStream* stream = SHCreateMemStream(
                    asset->bytes.empty() ? nullptr : asset->bytes.data(),
                    static_cast<UINT>(asset->bytes.size()));
                const bool is_html = asset->mime.starts_with("text/html");
                const std::string headers = security::format_response_headers(asset->mime, self->config.csp, is_html);
                self->env->CreateWebResourceResponse(
                    stream, 200, L"OK", native::utf8_to_wide(headers).c_str(), response.put());
                if (stream) stream->Release();
                if (response) args->put_Response(response.get());
                return S_OK;
            });
        webview->add_WebResourceRequested(res_handler, &token);
        res_handler->Release();

        auto* msg_handler = detail::make_com_handler<ICoreWebView2WebMessageReceivedEventHandler>(
            IID_ICoreWebView2WebMessageReceivedEventHandler,
            [weak](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                auto self = weak.lock();
                if (!self || self->destroyed.load()) {
                    return S_OK;
                }
                LPWSTR raw_source = nullptr;
                if (FAILED(args->get_Source(&raw_source)) || !raw_source) {
                    return S_OK;
                }
                const std::string source = native::wide_to_utf8(raw_source);
                CoTaskMemFree(raw_source);
                if (!security::is_trusted_origin(source)) {
                    return S_OK;
                }

                LPWSTR raw_json = nullptr;
                if (FAILED(args->get_WebMessageAsJson(&raw_json)) || !raw_json) {
                    return S_OK;
                }
                const std::string json_str = native::wide_to_utf8(raw_json);
                CoTaskMemFree(raw_json);

                try {
                    auto parsed = nlohmann::json::parse(json_str);
                    if (self->on_message && parsed.is_object()) {
                        self->on_message(parsed);
                    }
                } catch (...) {

                }
                return S_OK;
            });
        webview->add_WebMessageReceived(msg_handler, &token);
        msg_handler->Release();

        auto* nav_completed_handler = detail::make_com_handler<ICoreWebView2NavigationCompletedEventHandler>(
            IID_ICoreWebView2NavigationCompletedEventHandler,
            [weak](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                auto self = weak.lock();
                if (!self || self->destroyed.load()) {
                    return S_OK;
                }
                LPWSTR raw_source = nullptr;
                std::string source;
                if (sender && SUCCEEDED(sender->get_Source(&raw_source)) && raw_source) {
                    source = native::wide_to_utf8(raw_source);
                    CoTaskMemFree(raw_source);
                }
                if (!security::is_trusted_origin(source)) {
                    return S_OK;
                }

                self->ready.store(true);
                self->flush_pending_messages();

                HWND h = self->hwnd.load();
                if (h) {
                    self->last_maximized = (IsZoomed(h) != FALSE);
                    self->emit_event("rhenium:maximized", nlohmann::json(self->last_maximized));
                }

                if (!self->on_ready_fired.exchange(true) && self->on_ready) {
                    self->on_ready();
                }
                return S_OK;
            });
        webview->add_NavigationCompleted(nav_completed_handler, &token);
        nav_completed_handler->Release();

        auto* nav_handler = detail::make_com_handler<ICoreWebView2NavigationStartingEventHandler>(
            IID_ICoreWebView2NavigationStartingEventHandler,
            [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                LPWSTR raw_uri = nullptr;
                if (FAILED(args->get_Uri(&raw_uri)) || !raw_uri) {
                    return S_OK;
                }
                const std::string uri = native::wide_to_utf8(raw_uri);
                CoTaskMemFree(raw_uri);

                if (security::is_external_navigation(uri)) {
                    args->put_Cancel(TRUE);
                    native::open_external_url(uri);
                } else if (!security::is_trusted_origin(uri)) {
                    args->put_Cancel(TRUE);
                }
                return S_OK;
            });
        webview->add_NavigationStarting(nav_handler, &token);
        nav_handler->Release();

        auto* new_win_handler = detail::make_com_handler<ICoreWebView2NewWindowRequestedEventHandler>(
            IID_ICoreWebView2NewWindowRequestedEventHandler,
            [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                args->put_Handled(TRUE);
                LPWSTR raw_uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&raw_uri)) && raw_uri) {
                    const std::string uri = native::wide_to_utf8(raw_uri);
                    CoTaskMemFree(raw_uri);
                    if (security::is_external_navigation(uri)) {
                        native::open_external_url(uri);
                    }
                }
                return S_OK;
            });
        webview->add_NewWindowRequested(new_win_handler, &token);
        new_win_handler->Release();
    }

    LRESULT handle_message(HWND h, UINT msg, WPARAM wparam, LPARAM lparam) {
        switch (msg) {
            case WM_SIZE: {
                resize_webview();
                const bool now_max = (IsZoomed(h) != FALSE);
                if (now_max != last_maximized) {
                    last_maximized = now_max;
                    if (ready.load()) {
                        emit_event("rhenium:maximized", nlohmann::json(now_max));
                    }
                }
                return 0;
            }

            case WM_GETMINMAXINFO: {
                auto* mmi = reinterpret_cast<MINMAXINFO*>(lparam);
                const UINT dpi = get_window_dpi_safe(h);
                const double scale = dpi > 0 ? (static_cast<double>(dpi) / 96.0) : 1.0;
                if (config.min_width > 0) {
                    mmi->ptMinTrackSize.x = static_cast<LONG>(config.min_width * scale);
                }
                if (config.min_height > 0) {
                    mmi->ptMinTrackSize.y = static_cast<LONG>(config.min_height * scale);
                }
                if (config.max_width > 0) {
                    mmi->ptMaxTrackSize.x = static_cast<LONG>(config.max_width * scale);
                }
                if (config.max_height > 0) {
                    mmi->ptMaxTrackSize.y = static_cast<LONG>(config.max_height * scale);
                }
                return 0;
            }

            case WM_DPICHANGED: {
                const auto* suggested = reinterpret_cast<const RECT*>(lparam);
                if (suggested) {
                    SetWindowPos(
                        h, nullptr,
                        suggested->left, suggested->top,
                        suggested->right - suggested->left,
                        suggested->bottom - suggested->top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    resize_webview();
                }
                return 0;
            }

            case WM_NCCALCSIZE: {
                if (config.frameless && wparam == TRUE) {
                    if (IsZoomed(h)) {
                        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                        HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
                        MONITORINFO mi{};
                        mi.cbSize = sizeof(mi);
                        if (GetMonitorInfoW(mon, &mi)) {
                            params->rgrc[0] = mi.rcWork;
                        }
                    }
                    return 0;
                }
                break;
            }

            case WM_NCHITTEST: {
                if (config.frameless && config.resizable && !IsZoomed(h)) {
                    RECT rc{};
                    GetWindowRect(h, &rc);
                    const int x = GET_X_LPARAM(lparam);
                    const int y = GET_Y_LPARAM(lparam);
                    const UINT dpi = get_window_dpi_safe(h);
                    const int border = (std::max)(6, static_cast<int>(6.0 * dpi / 96.0));

                    const bool left = x >= rc.left && x < rc.left + border;
                    const bool right = x <= rc.right && x > rc.right - border;
                    const bool top = y >= rc.top && y < rc.top + border;
                    const bool bottom = y <= rc.bottom && y > rc.bottom - border;

                    if (top && left) return HTTOPLEFT;
                    if (top && right) return HTTOPRIGHT;
                    if (bottom && left) return HTBOTTOMLEFT;
                    if (bottom && right) return HTBOTTOMRIGHT;
                    if (left) return HTLEFT;
                    if (right) return HTRIGHT;
                    if (top) return HTTOP;
                    if (bottom) return HTBOTTOM;
                }
                break;
            }

            case WM_RHENIUM_DISPATCH:
                if (on_dispatch) {
                    on_dispatch();
                }
                return 0;

            case WM_DESTROY:
                destroyed.store(true);
                ready.store(false);
                SetWindowLongPtrW(h, GWLP_USERDATA, 0);
                hwnd.store(nullptr);
                if (controller) {
                    controller->Close();
                }
                webview.reset();
                controller.reset();
                env.reset();
                PostQuitMessage(0);
                return 0;

            default:
                break;
        }
        return DefWindowProcW(h, msg, wparam, lparam);
    }

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
        Impl* self = nullptr;
        if (msg == WM_NCCREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<Impl*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd.store(hwnd);
        } else {
            self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self) {
            return self->handle_message(hwnd, msg, wparam, lparam);
        }
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
};

Window::Window(const Config& config, const AssetResolver& resolver)
    : impl_(std::make_shared<Impl>(config, resolver)) {}

Window::~Window() = default;

void Window::set_on_message(MessageCallback cb) { impl_->on_message = std::move(cb); }
void Window::set_on_ready(ReadyCallback cb) { impl_->on_ready = std::move(cb); }
void Window::set_dispatch_handler(DispatchCallback cb) { impl_->on_dispatch = std::move(cb); }

bool Window::create() {
    enable_per_monitor_dpi();

    static std::once_flag class_once;
    static ATOM class_atom = 0;
    const HINSTANCE hinst = GetModuleHandleW(nullptr);
    std::call_once(class_once, [hinst]() {
        static HBRUSH bg_brush = CreateSolidBrush(RGB(15, 17, 23));
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = &Impl::wnd_proc;
        wc.hInstance = hinst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        wc.hbrBackground = bg_brush;
        wc.lpszClassName = kWindowClassName;
        class_atom = RegisterClassExW(&wc);
    });
    if (class_atom == 0) {
        return false;
    }

    DWORD style = 0;
    if (impl_->config.frameless) {
        style = WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU;
        if (impl_->config.resizable) {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }
    } else {
        style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        if (impl_->config.resizable) {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }
    }

    const UINT initial_dpi = get_system_dpi_safe();
    const double initial_scale = static_cast<double>(initial_dpi) / 96.0;
    RECT rect{
        0, 0,
        static_cast<LONG>(impl_->config.width * initial_scale),
        static_cast<LONG>(impl_->config.height * initial_scale)
    };
    if (!impl_->config.frameless) {
        adjust_window_rect_for_dpi(&rect, style, FALSE, 0, initial_dpi);
    }

    const int win_w = rect.right - rect.left;
    const int win_h = rect.bottom - rect.top;
    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const int x = (screen_w > win_w) ? (screen_w - win_w) / 2 : CW_USEDEFAULT;
    const int y = (screen_h > win_h) ? (screen_h - win_h) / 2 : CW_USEDEFAULT;

    const std::wstring title_w = native::utf8_to_wide(impl_->config.title);
    HWND hwnd = CreateWindowExW(
        0,
        kWindowClassName,
        title_w.c_str(),
        style,
        x, y, win_w, win_h,
        nullptr, nullptr, hinst, impl_.get());
    if (!hwnd) {
        return false;
    }

    apply_dark_titlebar(hwnd, impl_->config.dark_titlebar);
    if (impl_->config.frameless) {
        MARGINS shadow{0, 0, 0, 1};
        DwmExtendFrameIntoClientArea(hwnd, &shadow);
        SetWindowPos(
            hwnd, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    impl_->init_webview2();
    return true;
}

int Window::run_event_loop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void Window::post_dispatch_signal() {
    HWND h = impl_->hwnd.load();
    if (h) {
        PostMessageW(h, WM_RHENIUM_DISPATCH, 0, 0);
    }
}

void Window::set_title(std::string_view title) {
    HWND h = impl_->hwnd.load();
    if (h) {
        SetWindowTextW(h, native::utf8_to_wide(title).c_str());
    }
}

void Window::minimize() {
    HWND h = impl_->hwnd.load();
    if (h) {
        ShowWindow(h, SW_MINIMIZE);
    }
}

void Window::toggle_maximize() {
    HWND h = impl_->hwnd.load();
    if (!h) return;
    if (IsZoomed(h)) {
        ShowWindow(h, SW_RESTORE);
    } else {
        ShowWindow(h, SW_MAXIMIZE);
    }
}

bool Window::is_maximized() const {
    HWND h = impl_->hwnd.load();
    return h && IsZoomed(h) != FALSE;
}

void Window::close() {
    HWND h = impl_->hwnd.load();
    if (h) {
        PostMessageW(h, WM_CLOSE, 0, 0);
    }
}

void Window::start_drag() {
    HWND h = impl_->hwnd.load();
    if (!h) return;
    ReleaseCapture();
    PostMessageW(h, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

void Window::start_resize(std::string_view edge) {
    HWND h = impl_->hwnd.load();
    if (!h || IsZoomed(h) || !impl_->config.frameless || !impl_->config.resizable) return;
    WPARAM ht = 0;
    if (edge == "w") ht = HTLEFT;
    else if (edge == "e") ht = HTRIGHT;
    else if (edge == "n") ht = HTTOP;
    else if (edge == "s") ht = HTBOTTOM;
    else if (edge == "nw") ht = HTTOPLEFT;
    else if (edge == "ne") ht = HTTOPRIGHT;
    else if (edge == "sw") ht = HTBOTTOMLEFT;
    else if (edge == "se") ht = HTBOTTOMRIGHT;
    if (ht != 0) {
        ReleaseCapture();
        SendMessageW(h, WM_NCLBUTTONDOWN, ht, 0);
    }
}

void Window::open_devtools() {
    if (impl_->webview && impl_->config.devtools) {
        impl_->webview->OpenDevToolsWindow();
    }
}

void Window::reload() {
    if (impl_->webview) {
        impl_->webview->Reload();
    }
}

void* Window::native_handle() const {
    return impl_->hwnd.load();
}

bool Window::is_ready() const {
    return impl_->ready.load();
}

void Window::post_json_message(const nlohmann::json& msg) {
    impl_->post_json(msg);
}

void Window::emit(std::string_view event, const nlohmann::json& payload) {
    impl_->emit_event(event, payload);
}

}
