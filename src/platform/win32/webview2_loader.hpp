#pragma once

#include <windows.h>
#include <objbase.h>
#include <eventtoken.h>
#include <WebView2.h>

#include <atomic>
#include <utility>

namespace rhenium::detail {

template <typename Sig>
struct invoke_args;

template <typename T, typename A1, typename A2>
struct invoke_args<HRESULT (STDMETHODCALLTYPE T::*)(A1, A2)> {
    using first = A1;
    using second = A2;
};

template <typename Interface, typename Fn>
class ComHandler final : public Interface {
public:
    using Arg1 = typename invoke_args<decltype(&Interface::Invoke)>::first;
    using Arg2 = typename invoke_args<decltype(&Interface::Invoke)>::second;

    ComHandler(IID iid, Fn fn) : iid_(iid), fn_(std::move(fn)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, iid_)) {
            *ppv = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++ref_count_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = --ref_count_;
        if (count == 0) {
            delete this;
        }
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(Arg1 a1, Arg2 a2) override {
        return fn_(a1, a2);
    }

private:
    ~ComHandler() = default;

    std::atomic<ULONG> ref_count_{1};
    IID iid_;
    Fn fn_;
};

template <typename Interface, typename Fn>
Interface* make_com_handler(IID iid, Fn&& fn) {
    return new ComHandler<Interface, std::decay_t<Fn>>(iid, std::forward<Fn>(fn));
}

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() { reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T** put() {
        reset();
        return &ptr_;
    }
    explicit operator bool() const { return ptr_ != nullptr; }

    void attach(T* p) {
        reset();
        ptr_ = p;
    }

    void reset() {
        if (ptr_) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

using CreateWebView2EnvFn = HRESULT(STDAPICALLTYPE*)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);

CreateWebView2EnvFn load_webview2_create_env();

}
