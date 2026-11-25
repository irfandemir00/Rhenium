#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace rhenium {

struct IpcResult {
    bool ok = false;
    nlohmann::json value = nullptr;
    std::string error;
};

namespace detail {

template <typename T>
struct function_traits : function_traits<decltype(&std::decay_t<T>::operator())> {};

template <typename R, typename... Args>
struct function_traits<R (*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename C, typename R, typename... Args>
struct function_traits<R (C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<std::decay_t<Args>...>;
    static constexpr size_t arity = sizeof...(Args);
};

template <typename T>
struct is_std_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_std_vector<std::vector<T, Alloc>> : std::true_type {};

template <typename Tuple, size_t... Is, typename Fn>
nlohmann::json invoke_with_json_args(Fn& fn, const nlohmann::json& args, std::index_sequence<Is...>) {
    constexpr size_t N = sizeof...(Is);
    if constexpr (N == 0) {
        (void)args;
        if constexpr (std::is_void_v<std::invoke_result_t<Fn>>) {
            fn();
            return nullptr;
        } else {
            return nlohmann::json(fn());
        }
    } else if constexpr (N == 1) {
        using Arg0 = std::tuple_element_t<0, Tuple>;
        Arg0 converted = [&]() -> Arg0 {
            if constexpr (std::is_same_v<Arg0, nlohmann::json>) {
                if (args.is_array() && args.size() == 1) {
                    return args[0];
                }
                return args;
            } else if constexpr (is_std_vector<Arg0>::value) {
                if (args.is_array() && args.size() == 1 && args[0].is_array()) {
                    return args[0].get<Arg0>();
                }
                return args.get<Arg0>();
            } else {
                const nlohmann::json* arg_ptr = &args;
                if (args.is_array()) {
                    if (args.size() != 1) {
                        throw std::invalid_argument("Expected 1 argument, got " + std::to_string(args.size()));
                    }
                    arg_ptr = &args[0];
                } else if (args.is_null()) {
                    throw std::invalid_argument("Expected 1 argument, got null");
                }
                return arg_ptr->get<Arg0>();
            }
        }();

        if constexpr (std::is_void_v<std::invoke_result_t<Fn, Arg0>>) {
            fn(std::move(converted));
            return nullptr;
        } else {
            return nlohmann::json(fn(std::move(converted)));
        }
    } else {
        if (!args.is_array() || args.size() != N) {
            const size_t got = args.is_array() ? args.size() : (args.is_null() ? 0 : 1);
            throw std::invalid_argument(
                "Expected " + std::to_string(N) + " arguments, got " + std::to_string(got));
        }
        if constexpr (std::is_void_v<std::invoke_result_t<Fn, std::tuple_element_t<Is, Tuple>...>>) {
            fn(args[Is].get<std::tuple_element_t<Is, Tuple>>()...);
            return nullptr;
        } else {
            return nlohmann::json(fn(args[Is].get<std::tuple_element_t<Is, Tuple>>()...));
        }
    }
}

struct CallableBase {
    virtual ~CallableBase() = default;
    virtual nlohmann::json call(const nlohmann::json& args) = 0;
};

template <typename Fn, typename Tuple, size_t Arity>
struct CallableModel final : CallableBase {
    explicit CallableModel(Fn fn) : func(std::move(fn)) {}

    nlohmann::json call(const nlohmann::json& args) override {
        return invoke_with_json_args<Tuple>(func, args, std::make_index_sequence<Arity>{});
    }

    Fn func;
};

}

class IpcRouter {
public:
    using HandlerFn = std::function<nlohmann::json(const nlohmann::json&)>;

    template <typename Fn>
    void register_command(std::string name, Fn&& fn, bool is_async = false) {
        using DecayedFn = std::decay_t<Fn>;
        using Traits = detail::function_traits<DecayedFn>;
        using Tuple = typename Traits::args_tuple;
        constexpr size_t Arity = Traits::arity;

        auto callable = std::make_shared<detail::CallableModel<DecayedFn, Tuple, Arity>>(
            std::forward<Fn>(fn));

        std::lock_guard<std::mutex> lock(mutex_);
        commands_.insert_or_assign(std::move(name), Entry{std::move(callable), is_async});
    }

    bool has_command(std::string_view name) const;
    bool is_async(std::string_view name) const;
    IpcResult invoke(std::string_view name, const nlohmann::json& args) const;

private:
    struct Entry {
        std::shared_ptr<detail::CallableBase> callable;
        bool async = false;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> commands_;
};

}
