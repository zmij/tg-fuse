#pragma once

#include <coroutine>
#include <exception>
#include <memory>
#include <variant>

namespace tg {

// Forward declaration
template <typename T>
class Task;

namespace detail {

// Promise type for Task coroutines
template <typename T>
class TaskPromise {
public:
    using Handle = std::coroutine_handle<TaskPromise<T>>;

    Task<T> get_return_object() noexcept;

    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<T>> h) noexcept {
                auto& promise = h.promise();
                if (promise.continuation_) {
                    return promise.continuation_;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }

    void unhandled_exception() noexcept { result_ = std::current_exception(); }

    // For co_return value
    void return_value(T value) noexcept { result_ = std::move(value); }

    T& result() {
        if (std::holds_alternative<std::exception_ptr>(result_)) {
            std::rethrow_exception(std::get<std::exception_ptr>(result_));
        }
        return std::get<T>(result_);
    }

    void set_continuation(std::coroutine_handle<> continuation) {
        continuation_ = continuation;
    }

private:
    std::variant<std::monostate, T, std::exception_ptr> result_;
    std::coroutine_handle<> continuation_;
};

// Specialisation for void
template <>
class TaskPromise<void> {
public:
    using Handle = std::coroutine_handle<TaskPromise<void>>;

    Task<void> get_return_object() noexcept;

    std::suspend_always initial_suspend() noexcept { return {}; }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }

            std::coroutine_handle<> await_suspend(std::coroutine_handle<TaskPromise<void>> h) noexcept {
                auto& promise = h.promise();
                if (promise.continuation_) {
                    return promise.continuation_;
                }
                return std::noop_coroutine();
            }

            void await_resume() noexcept {}
        };
        return FinalAwaiter{};
    }

    void unhandled_exception() noexcept { exception_ = std::current_exception(); }

    void return_void() noexcept {}

    void result() {
        if (exception_) {
            std::rethrow_exception(exception_);
        }
    }

    void set_continuation(std::coroutine_handle<> continuation) {
        continuation_ = continuation;
    }

private:
    std::exception_ptr exception_;
    std::coroutine_handle<> continuation_;
};

}  // namespace detail

// Awaitable task type
template <typename T = void>
class Task {
public:
    using promise_type = detail::TaskPromise<T>;
    using Handle = typename promise_type::Handle;

    explicit Task(Handle handle) : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }

    // Delete copy operations
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    // Awaitable interface
    bool await_ready() const noexcept { return false; }

    std::coroutine_handle<> await_suspend(std::coroutine_handle<> continuation) noexcept {
        // When this task is co_awaited, set up continuation to be resumed when this task completes
        handle_.promise().set_continuation(continuation);
        return handle_;
    }

    T await_resume() {
        if constexpr (std::is_void_v<T>) {
            handle_.promise().result();
        } else {
            return handle_.promise().result();
        }
    }

    // Resume the task manually
    void resume() {
        if (handle_ && !handle_.done()) {
            handle_.resume();
        }
    }

    // Check if task is done
    bool done() const { return handle_.done(); }

    // Get the result (will throw if task threw an exception)
    T get_result() {
        while (!handle_.done()) {
            handle_.resume();
        }

        if constexpr (std::is_void_v<T>) {
            handle_.promise().result();
        } else {
            return handle_.promise().result();
        }
    }

private:
    Handle handle_;

    friend class detail::TaskPromise<T>;
};

// Implementation of promise's get_return_object
namespace detail {

template <typename T>
Task<T> TaskPromise<T>::get_return_object() noexcept {
    return Task<T>{Handle::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept {
    return Task<void>{Handle::from_promise(*this)};
}

}  // namespace detail

// Promise type for bridging TDLib callbacks to coroutines
template <typename T>
class TdPromise {
public:
    TdPromise() = default;

    // Get an awaitable that will suspend until set_value/set_exception is called
    auto operator co_await() {
        struct Awaiter {
            TdPromise& promise;

            bool await_ready() const noexcept { return promise.ready_; }

            void await_suspend(std::coroutine_handle<> continuation) noexcept {
                promise.continuation_ = continuation;
            }

            T await_resume() {
                if (promise.exception_) {
                    std::rethrow_exception(promise.exception_);
                }
                if constexpr (!std::is_void_v<T>) {
                    return std::move(promise.value_);
                }
            }
        };

        return Awaiter{*this};
    }

    // Called by TDLib callback to complete the promise with a value
    void set_value(T value) requires(!std::is_void_v<T>) {
        value_ = std::move(value);
        ready_ = true;
        if (continuation_) {
            continuation_.resume();
        }
    }

    // Specialisation for void
    void set_value() requires(std::is_void_v<T>) {
        ready_ = true;
        if (continuation_) {
            continuation_.resume();
        }
    }

    // Called by TDLib callback to complete the promise with an exception
    void set_exception(std::exception_ptr exception) {
        exception_ = exception;
        ready_ = true;
        if (continuation_) {
            continuation_.resume();
        }
    }

private:
    bool ready_ = false;
    T value_{};  // Only used when T is not void
    std::exception_ptr exception_;
    std::coroutine_handle<> continuation_;
};

// Specialisation for void type
template <>
class TdPromise<void> {
public:
    TdPromise() = default;

    auto operator co_await() {
        struct Awaiter {
            TdPromise& promise;

            bool await_ready() const noexcept { return promise.ready_; }

            void await_suspend(std::coroutine_handle<> continuation) noexcept {
                promise.continuation_ = continuation;
            }

            void await_resume() {
                if (promise.exception_) {
                    std::rethrow_exception(promise.exception_);
                }
            }
        };

        return Awaiter{*this};
    }

    void set_value() {
        ready_ = true;
        if (continuation_) {
            continuation_.resume();
        }
    }

    void set_exception(std::exception_ptr exception) {
        exception_ = exception;
        ready_ = true;
        if (continuation_) {
            continuation_.resume();
        }
    }

private:
    bool ready_ = false;
    std::exception_ptr exception_;
    std::coroutine_handle<> continuation_;
};

}  // namespace tg
