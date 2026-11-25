#pragma once

#include <functional>
#include <memory>
#include <string_view>
#include <nlohmann/json.hpp>

#include "rhenium/core/assets.hpp"
#include "rhenium/core/config.hpp"

namespace rhenium {

class Window {
public:
    using MessageCallback = std::function<void(const nlohmann::json& message)>;
    using ReadyCallback = std::function<void()>;
    using DispatchCallback = std::function<void()>;

    Window(const Config& config, const AssetResolver& resolver);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void set_on_message(MessageCallback cb);
    void set_on_ready(ReadyCallback cb);
    void set_dispatch_handler(DispatchCallback cb);

    bool create();
    int run_event_loop();
    void post_dispatch_signal();

    void set_title(std::string_view title);
    void minimize();
    void toggle_maximize();
    bool is_maximized() const;
    void close();
    void start_drag();
    void open_devtools();
    void reload();
    void* native_handle() const;
    bool is_ready() const;

    void post_json_message(const nlohmann::json& msg);
    void emit(std::string_view event, const nlohmann::json& payload = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
