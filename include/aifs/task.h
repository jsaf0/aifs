#pragma once

#include <coroutine>
#include <variant>
#include <utility>

#include <fmt/core.h>

#include "handle.h"

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

        explicit task(coro_handle h) noexcept : handle_(h) {}
        task(task&& other) noexcept : handle_{std::exchange(other.handle_, {})} {}

        ~task() {}

        struct awaitable_base {
            constexpr bool await_ready() {
                if (self_) {
                    fmt::print("{} is done: {}\n", self_.promise().id_, self_.done());
                    return self_.done();
                }
                return true;
            }

            template <typename Promise>
            auto await_suspend(std::coroutine_handle<Promise> resumer) const noexcept {
                fmt::print("[{}] await_suspend\n", self_.promise().id_);
                self_.promise().continuation_ = &resumer.promise();
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
            fmt::print("co_await on task {}\n", handle_.promise().id_);
            return awaiter{handle_};
        }

        struct promise : handle, Result<ReturnType> {
            task get_return_object() {
                return task{coro_handle::from_promise(*this)};
            }

            auto initial_suspend() noexcept {
                fmt::print("[{}] initial suspend\n", id_);
                return std::suspend_always{};
            }

            struct final_awaiter {
                constexpr bool await_ready() const noexcept { return false; }
                template <typename Promise>
                constexpr void await_suspend(std::coroutine_handle<Promise> h) const noexcept {
                    if (auto cont = h.promise().continuation_) {
                        cont->run();
                    }
                }
                constexpr void await_resume() const noexcept {}
            };

            auto final_suspend() noexcept {
                fmt::print("[{}] final suspend\n", id_);
                return final_awaiter{};
            }

            void unhandled_exception() {}

            void run() override {
                fmt::print("[{}] run\n", id_);
                coro_handle::from_promise(*this).resume();
            }

            handle* continuation_{};
        };


    public:
        // TODO: Should not be public
        coro_handle handle_;
    };

    template <typename ReturnType>
    using awaitable = task<ReturnType>;
}