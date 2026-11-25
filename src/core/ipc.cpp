#include "rhenium/core/ipc.hpp"

namespace rhenium {

bool IpcRouter::has_command(std::string_view name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return commands_.find(std::string(name)) != commands_.end();
}

bool IpcRouter::is_async(std::string_view name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = commands_.find(std::string(name));
    return it != commands_.end() && it->second.async;
}

IpcResult IpcRouter::invoke(std::string_view name, const nlohmann::json& args) const {
    std::shared_ptr<detail::CallableBase> callable;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = commands_.find(std::string(name));
        if (it == commands_.end()) {
            return IpcResult{false, nullptr, "Unknown command: " + std::string(name)};
        }
        callable = it->second.callable;
    }

    try {
        nlohmann::json result = callable->call(args);
        return IpcResult{true, std::move(result), {}};
    } catch (const std::exception& e) {
        return IpcResult{false, nullptr, e.what()};
    } catch (...) {
        return IpcResult{false, nullptr, "Unknown C++ exception in command: " + std::string(name)};
    }
}

}
