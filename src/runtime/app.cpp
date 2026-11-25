#include "rhenium/runtime/app.hpp"

#include "bridge/builtin_commands.hpp"
#include "platform/win32/file_watcher.hpp"
#include "rhenium/platform/native.hpp"

#include <windows.h>
#include <objbase.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace rhenium {

namespace {

bool should_prefer_embedded() {
#if defined(RHENIUM_EMBEDDED_ASSETS) && RHENIUM_EMBEDDED_ASSETS
    return true;
#else
    return EmbeddedAssetRegistry::instance().count() > 0;
#endif
}

std::filesystem::path get_executable_dir() {
    wchar_t buf[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return std::filesystem::path(buf).parent_path();
    }
    return {};
}

std::filesystem::path resolve_ui_directory(const Config& config) {
    std::error_code ec;

    const std::string registered_dev_dir = get_default_dev_ui_dir();
    if (!registered_dev_dir.empty()) {
        std::filesystem::path dev_dir(native::utf8_to_wide(registered_dev_dir));
        if (std::filesystem::exists(dev_dir, ec)) {
            return dev_dir;
        }
    }

    std::filesystem::path configured(native::utf8_to_wide(config.ui_dir));
    if (!configured.empty() && std::filesystem::exists(configured, ec)) {
        return configured;
    }

    if (configured.is_relative()) {
        const auto exe_dir = get_executable_dir();
        if (!exe_dir.empty()) {
            const auto beside_exe = exe_dir / configured;
            if (std::filesystem::exists(beside_exe, ec)) {
                return beside_exe;
            }
        }
    }

    return configured;
}

}

struct App::RuntimeImpl {
    std::mutex dispatch_mutex;
    std::queue<std::function<void()>> dispatch_queue;

    std::mutex worker_mutex;
    std::condition_variable worker_cv;
    std::queue<std::function<void()>> worker_queue;
    std::vector<std::thread> workers;
    bool stopping = false;

    std::unique_ptr<detail::FileWatcher> watcher;
    std::function<void()> user_on_ready;

    void start_workers() {
        if (!workers.empty()) return;
        const unsigned count = std::max(2u, std::thread::hardware_concurrency());
        workers.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(worker_mutex);
                        worker_cv.wait(lock, [this]() {
                            return stopping || !worker_queue.empty();
                        });
                        if (stopping && worker_queue.empty()) {
                            return;
                        }
                        task = std::move(worker_queue.front());
                        worker_queue.pop();
                    }
                    if (task) {
                        task();
                    }
                }
            });
        }
    }

    void enqueue_work(std::function<void()> fn) {
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            if (stopping) return;
            worker_queue.push(std::move(fn));
        }
        worker_cv.notify_one();
    }

    void stop_workers() {
        {
            std::lock_guard<std::mutex> lock(worker_mutex);
            stopping = true;
        }
        worker_cv.notify_all();
        for (auto& t : workers) {
            if (t.joinable()) {
                t.join();
            }
        }
        workers.clear();
    }
};

App::App(Config config)
    : config_(std::move(config)),
      resolver_(resolve_ui_directory(config_), should_prefer_embedded()),
      window_(config_, resolver_),
      runtime_(std::make_unique<RuntimeImpl>()) {
    detail::register_builtin_commands(*this);

    window_.set_on_message([this](const nlohmann::json& msg) {
        handle_frontend_message(msg);
    });
    window_.set_dispatch_handler([this]() {
        drain_dispatch_queue();
    });
    window_.set_on_ready([this]() {
        if (runtime_->user_on_ready) {
            runtime_->user_on_ready();
        }
    });
}

App::~App() {
    if (runtime_) {
        if (runtime_->watcher) {
            runtime_->watcher->stop();
        }
        runtime_->stop_workers();
    }
}

App App::from_config_file(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        return App(Config::from_file(path));
    }
    if (path.is_relative()) {
        const auto exe_dir = get_executable_dir();
        if (!exe_dir.empty()) {
            const auto beside_exe = exe_dir / path;
            if (std::filesystem::exists(beside_exe, ec)) {
                return App(Config::from_file(beside_exe));
            }
        }
    }
    return App(Config::from_file(path));
}

void App::dispatch(std::function<void()> fn) {
    if (!fn) return;
    {
        std::lock_guard<std::mutex> lock(runtime_->dispatch_mutex);
        runtime_->dispatch_queue.push(std::move(fn));
    }
    window_.post_dispatch_signal();
}

void App::on_ready(std::function<void()> fn) {
    runtime_->user_on_ready = std::move(fn);
}

void App::drain_dispatch_queue() {
    std::queue<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lock(runtime_->dispatch_mutex);
        local.swap(runtime_->dispatch_queue);
    }
    while (!local.empty()) {
        auto fn = std::move(local.front());
        local.pop();
        if (fn) {
            fn();
        }
    }
}

void App::handle_internal_command(const std::string& cmd) {
    if (cmd == "__rh_window_start_drag") {
        window_.start_drag();
    } else if (cmd == "__rh_window_toggle_maximize") {
        window_.toggle_maximize();
    }
}

void App::handle_frontend_message(const nlohmann::json& msg) {
    const std::string kind = msg.value("kind", "");
    if (kind == "internal") {
        handle_internal_command(msg.value("cmd", ""));
        return;
    }

    if (kind == "invoke") {
        const nlohmann::json id = msg.contains("id") ? msg["id"] : nlohmann::json(nullptr);
        const std::string cmd = msg.value("cmd", "");
        const nlohmann::json args = msg.contains("args") ? msg["args"] : nlohmann::json::array();

        if (router_.is_async(cmd)) {
            runtime_->enqueue_work([this, id, cmd, args]() {
                const IpcResult res = router_.invoke(cmd, args);
                dispatch([this, id, res]() {
                    window_.post_json_message(nlohmann::json{
                        {"kind", "response"},
                        {"id", id},
                        {"ok", res.ok},
                        {"value", res.value},
                        {"error", res.error}
                    });
                });
            });
            return;
        }

        const IpcResult res = router_.invoke(cmd, args);
        window_.post_json_message(nlohmann::json{
            {"kind", "response"},
            {"id", id},
            {"ok", res.ok},
            {"value", res.value},
            {"error", res.error}
        });
    }
}

int App::run() {
    const HRESULT com_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool owns_com = SUCCEEDED(com_hr);

    runtime_->start_workers();

    if (!window_.create()) {
        if (owns_com) {
            CoUninitialize();
        }
        return 1;
    }

    if (!resolver_.prefer_embedded() && config_.hot_reload) {
        runtime_->watcher = std::make_unique<detail::FileWatcher>(
            resolver_.ui_dir(),
            [this](bool css_only) {
                dispatch([this, css_only]() {
                    window_.post_json_message(nlohmann::json{
                        {"kind", css_only ? "reload_css" : "reload_page"}
                    });
                });
            });
        runtime_->watcher->start();
    }

    const int exit_code = window_.run_event_loop();

    if (runtime_->watcher) {
        runtime_->watcher->stop();
    }
    runtime_->stop_workers();

    if (owns_com) {
        CoUninitialize();
    }
    return exit_code;
}

}
