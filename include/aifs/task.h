#pragma once

#include <coroutine>
#include <utility>
#include <variant>

#include "non_copyable.h"

namespace aifs {
template <typename T>
struct Result {
    template <typename R>
    constexpr void return_value(R&& value) noexcept
    {
        m_result.template emplace<T>(std::forward<R>(value));
    }

    constexpr T result()
    {
        if (auto res = std::get_if<T>(&m_result)) {
            return *res;
        }
        return T { -1 };
    }

    std::variant<std::monostate, T, std::exception_ptr> m_result;
};

template <>
struct Result<void> {
    void return_void() noexcept { }
    void result() { }
};

template <typename ReturnType = void>
struct Task : private NonCopyable {
    struct Promise;
    using promise_type = Promise;
    using CoroHandle = std::coroutine_handle<promise_type>;

    template <typename Fut>
    friend struct scheduled_task;

    Task() noexcept
        : m_handle { nullptr }
    {
    }

    explicit Task(CoroHandle h) noexcept
        : m_handle(h)
    {
    }

    Task(Task&& other) noexcept
        : m_handle { std::exchange(other.m_handle, {}) }
    {
    }

    ~Task()
    {
        if (m_handle) {
            m_handle.destroy();
        }
    }

    struct AwaitableBase {
        constexpr bool await_ready()
        {
            if (m_self) {
                return m_self.done();
            }
            return true;
        }

        auto await_suspend(std::coroutine_handle<> h) const noexcept
        {
            m_self.promise().m_continuation = h;
            return m_self;
        }

        CoroHandle m_self;
    };

    auto operator co_await() const noexcept
    {
        struct Awaiter : AwaitableBase {
            auto await_resume() const
            {
                return AwaitableBase::m_self.promise().result();
            }
        };
        return Awaiter { m_handle };
    }

    struct Promise : Result<ReturnType> {
        Task get_return_object()
        {
            return Task { CoroHandle::from_promise(*this) };
        }

        auto initial_suspend() noexcept { return std::suspend_always {}; }

        struct FinalAwaiter {
            constexpr bool await_ready() const noexcept { return false; }
            template <typename Promise>
            constexpr void await_suspend(
                std::coroutine_handle<Promise> h) const noexcept
            {
                if (auto cont = h.promise().m_continuation) {
                    cont.resume();
                }
            }
            constexpr void await_resume() const noexcept { }
        };

        auto final_suspend() noexcept { return FinalAwaiter {}; }

        void unhandled_exception() { }

        void perform() { CoroHandle::from_promise(*this).resume(); }

        std::coroutine_handle<> m_continuation;
    };

public:
    // TODO: Should not be public
    CoroHandle m_handle;
};
} // namespace aifs
