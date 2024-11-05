#pragma once

#include <coroutine>
#include <variant>
#include <utility>

#include "non_copyable.h"

namespace aifs {
    template <typename T>
    struct Result {
        template <typename R>
        constexpr void return_value(R&& value) noexcept {
            result_.template emplace<T>(std::forward<R>(value));
        }

        constexpr T result() {
            if (auto res = std::get_if<T>(&result_)) {
                return *res;
            }
            return T{-1};
        }

        std::variant<std::monostate, T, std::exception_ptr> result_;
    };

    template <>
    struct Result<void> {
        void return_void() noexcept {
        }

        void result() {}
    };

    template <typename ReturnType = void>
    struct task : private non_copyable {

        struct promise;
        using promise_type = promise;
        using coro_handle = std::coroutine_handle<promise_type>;

        template<typename Fut>
        friend struct scheduled_task;

        task() noexcept
            : handle_{nullptr}
        {
        }

        explicit task(coro_handle h) noexcept
            : handle_(h)
        {
        }

        task(task&& other) noexcept
            : handle_{std::exchange(other.handle_, {})}
        {
        }

        ~task() {
            if (handle_) {
                handle_.destroy();
            }
        }

        struct awaitable_base {
            constexpr bool await_ready() {
                if (self_) {
                    return self_.done();
                }
                return true;
            }

            auto await_suspend(std::coroutine_handle<> h) const noexcept {
                self_.promise().continuation_ = h;
                return self_;
            }

            coro_handle self_;
        };

        auto operator co_await () const noexcept {
            struct awaiter : awaitable_base {
                auto await_resume() const {
                    return awaitable_base::self_.promise().result();
                }
            };
            return awaiter{handle_};
        }

        struct promise : Result<ReturnType> {
            task get_return_object() {
                return task{coro_handle::from_promise(*this)};
            }

            auto initial_suspend() noexcept {
                return std::suspend_always{};
            }

            struct final_awaiter {
                constexpr bool await_ready() const noexcept { return false; }
                template <typename Promise>
                constexpr void await_suspend(std::coroutine_handle<Promise> h) const noexcept {
                    if (auto cont = h.promise().continuation_) {
                        cont.resume();
                    }
                }
                constexpr void await_resume() const noexcept {}
            };

            auto final_suspend() noexcept {
                return final_awaiter{};
            }

            void unhandled_exception() {}

            void perform() {
                coro_handle::from_promise(*this).resume();
            }

            std::coroutine_handle<> continuation_;
        };


    public:
        // TODO: Should not be public
        coro_handle handle_;
    };
}
