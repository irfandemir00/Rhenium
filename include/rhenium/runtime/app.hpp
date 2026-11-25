#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "rhenium/core/assets.hpp"
#include "rhenium/core/config.hpp"
#include "rhenium/core/ipc.hpp"
#include "rhenium/platform/window.hpp"

namespace rhenium {

class App {
public:
    explicit App(Config config = {});
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    static App from_config_file(const std::filesystem::path& path = "rhenium.json");

    template <typename Fn>
    App& command(std::string name, Fn&& fn) {
        router_.register_command(std::move(name), std::forward<Fn>(fn), false);
        return *this;
    }

    template <typename Fn>
    App& async_command(std::string name, Fn&& fn) {
        router_.register_command(std::move(name), std::forward<Fn>(fn), true);
        return *this;
    }

    Window& window() { return window_; }
    const Window& window() const { return window_; }

    const Config& config() const { return config_; }
    const AssetResolver& resolver() const { return resolver_; }

    void dispatch(std::function<void()> fn);
    void on_ready(std::function<void()> fn);
    int run();

private:
    struct RuntimeImpl;

    void handle_frontend_message(const nlohmann::json& msg);
    void handle_internal_command(const std::string& cmd);
    void drain_dispatch_queue();

    Config config_;
    AssetResolver resolver_;
    IpcRouter router_;
    Window window_;
    std::unique_ptr<RuntimeImpl> runtime_;
};

}
