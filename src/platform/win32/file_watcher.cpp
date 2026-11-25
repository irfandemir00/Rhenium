#include "file_watcher.hpp"

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <string>
#include <vector>

namespace rhenium::detail {

namespace {

bool is_css_filename(std::wstring_view filename) {
    std::wstring lower(filename);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(std::towlower(c));
    });
    return lower.ends_with(L".css");
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
    std::vector<BYTE> buffer(16 * 1024);

    while (running_.load()) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = io_event;
        ResetEvent(io_event);

        DWORD bytes_returned = 0;
        const BOOL ok = ReadDirectoryChangesW(
            dir_handle,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            TRUE,
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION,
            &bytes_returned,
            &overlapped,
            nullptr);
        if (!ok) {
            break;
        }

        HANDLE handles[2] = {stop_event_, io_event};
        const DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 1) {
            CancelIo(dir_handle);
            break;
        }

        if (!GetOverlappedResult(dir_handle, &overlapped, &bytes_returned, FALSE) || bytes_returned == 0) {
            continue;
        }

        bool css_only = true;
        size_t offset = 0;
        while (offset < bytes_returned) {
            const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
            const std::wstring_view fname(info->FileName, info->FileNameLength / sizeof(wchar_t));
            if (!is_css_filename(fname)) {
                css_only = false;
            }
            if (info->NextEntryOffset == 0) {
                break;
            }
            offset += info->NextEntryOffset;
        }

        if (WaitForSingleObject(stop_event_, 80) == WAIT_OBJECT_0) {
            break;
        }

        if (running_.load() && callback_) {
            callback_(css_only);
        }
    }

    if (io_event) {
        CloseHandle(io_event);
    }
    CloseHandle(dir_handle);
}

}
