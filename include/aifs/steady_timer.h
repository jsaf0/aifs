#pragma once

#include "event_loop.h"
#include "task.h"

namespace aifs {
    class steady_timer {
    private:
        struct awaitable;

    public:
        steady_timer(io_context& ctx, std::chrono::milliseconds when)
            : ctx_{ctx}, when_{std::move(when)} {
        }

        auto async_wait() {
            return awaitable{this};
        }

        auto operator co_await () noexcept {
            return async_wait();
        }

    private:
        struct awaitable : operation {
            awaitable(steady_timer* self)
                : self_{self}, continuation_{nullptr} {}

            constexpr bool await_ready() const noexcept {
                return false;
            }
            constexpr void await_resume() const noexcept {
            }
            void await_suspend(std::coroutine_handle<> h) noexcept {
                continuation_ = h;
                self_->ctx_.call_later(self_->when_, this);
            }

            void perform() {
                if (continuation_) {
                    continuation_.resume();
                }
            }

            steady_timer* self_;
            std::coroutine_handle<> continuation_;
        };

    private:
        io_context& ctx_;
        std::chrono::milliseconds when_;
    };
}
