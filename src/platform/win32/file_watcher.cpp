#include "file_watcher.hpp"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <string>
#include <vector>

namespace rhenium::detail {

namespace {

bool is_ignored_temp_filename(std::wstring_view path) {
    const size_t slash = path.find_last_of(L"/\\");
    const std::wstring_view name = (slash == std::wstring_view::npos) ? path : path.substr(slash + 1);
    if (name.empty() || name.back() == L'~' || name.starts_with(L".#")) {
        return true;
    }
    std::wstring lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return lower.ends_with(L".tmp") ||
           lower.ends_with(L".swp") ||
           lower.ends_with(L".swx") ||
           lower.ends_with(L".bak") ||
           lower.ends_with(L".part") ||
           lower.ends_with(L".crdownload");
}

bool is_css_filename(std::wstring_view filename) {
    std::wstring lower(filename);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return lower.ends_with(L".css");
}

void collect_changes(const BYTE* data, DWORD bytes_returned, bool& any_relevant, bool& css_only) {
    size_t offset = 0;
    while (offset < bytes_returned) {
        const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(data + offset);
        const std::wstring_view fname(info->FileName, info->FileNameLength / sizeof(wchar_t));
        if (!is_ignored_temp_filename(fname)) {
            any_relevant = true;
            if (!is_css_filename(fname)) {
                css_only = false;
            }
        }
        if (info->NextEntryOffset == 0) {
            break;
        }
        offset += info->NextEntryOffset;
    }
}

}

FileWatcher::FileWatcher(std::filesystem::path dir, ChangeCallback cb)
    : dir_(std::move(dir)), callback_(std::move(cb)) {}

FileWatcher::~FileWatcher() {
    stop();
}

void FileWatcher::start() {
    if (running_.exchange(true)) {
        return;
    }
    std::error_code ec;
    if (dir_.empty() || !std::filesystem::exists(dir_, ec)) {
        running_.store(false);
        return;
    }
    stop_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stop_event_) {
        running_.store(false);
        return;
    }
    thread_ = std::thread(&FileWatcher::watch_loop, this);
}

void FileWatcher::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (stop_event_) {
        SetEvent(stop_event_);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    if (stop_event_) {
        CloseHandle(stop_event_);
        stop_event_ = nullptr;
    }
}

void FileWatcher::watch_loop() {
    HANDLE dir_handle = CreateFileW(
        dir_.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (dir_handle == INVALID_HANDLE_VALUE) {
        running_.store(false);
        return;
    }

    HANDLE io_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!io_event) {
        CloseHandle(dir_handle);
        running_.store(false);
        return;
    }
    std::vector<BYTE> buffer(16 * 1024);

    auto issue_read = [&](OVERLAPPED& overlapped) -> BOOL {
        overlapped = OVERLAPPED{};
        overlapped.hEvent = io_event;
        ResetEvent(io_event);
        DWORD bytes_returned = 0;
        return ReadDirectoryChangesW(
            dir_handle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
            &bytes_returned,
            &overlapped,
            nullptr);
    };

    auto cancel_and_drain = [&](OVERLAPPED& overlapped) {
        CancelIo(dir_handle);
        DWORD dummy = 0;
        GetOverlappedResult(dir_handle, &overlapped, &dummy, TRUE);
    };

    while (running_.load()) {
        OVERLAPPED overlapped{};
        if (!issue_read(overlapped)) {
            break;
        }

        HANDLE handles[2] = {stop_event_, io_event};
        const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 1) {
            cancel_and_drain(overlapped);
            break;
        }

        DWORD bytes_returned = 0;
        if (!GetOverlappedResult(dir_handle, &overlapped, &bytes_returned, FALSE) || bytes_returned == 0) {
            continue;
        }

        bool any_relevant = false;
        bool css_only = true;
        collect_changes(buffer.data(), bytes_returned, any_relevant, css_only);
        if (!any_relevant) {
            continue;
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
        bool stop_requested = false;
        while (running_.load()) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                break;
            }
            const DWORD remaining_ms = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
            if (remaining_ms == 0) {
                break;
            }

            if (!issue_read(overlapped)) {
                break;
            }
            const DWORD sub_wait = WaitForMultipleObjects(2, handles, FALSE, remaining_ms);
            if (sub_wait == WAIT_OBJECT_0) {
                cancel_and_drain(overlapped);
                stop_requested = true;
                break;
            }
            if (sub_wait == WAIT_OBJECT_0 + 1) {
                DWORD more_bytes = 0;
                if (GetOverlappedResult(dir_handle, &overlapped, &more_bytes, FALSE) && more_bytes > 0) {
                    collect_changes(buffer.data(), more_bytes, any_relevant, css_only);
                }
                continue;
            }
            CancelIo(dir_handle);
            DWORD trailing_bytes = 0;
            if (GetOverlappedResult(dir_handle, &overlapped, &trailing_bytes, TRUE) && trailing_bytes > 0) {
                collect_changes(buffer.data(), trailing_bytes, any_relevant, css_only);
            }
            break;
        }

        if (stop_requested || !running_.load()) {
            break;
        }

        if (callback_) {
            callback_(css_only);
        }
    }

    CloseHandle(io_event);
    CloseHandle(dir_handle);
}

}
