#pragma once

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <thread>

namespace rhenium::detail {

class FileWatcher {
public:
    using ChangeCallback = std::function<void(bool css_only)>;

    FileWatcher(std::filesystem::path dir, ChangeCallback cb);
    ~FileWatcher();

    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    void start();
    void stop();

private:
    void watch_loop();

    std::filesystem::path dir_;
    ChangeCallback callback_;
    HANDLE stop_event_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread thread_;
};

}
