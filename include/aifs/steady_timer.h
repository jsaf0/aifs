#pragma once

#include "event_loop.h"
#include "task.h"

namespace aifs {
    class steady_timer {
    public:
        steady_timer(io_context& ctx, std::chrono::milliseconds when)
            : ctx_{ctx}, when_{std::move(when)} {
        }

        auto async_wait() {
            return steady_timer_awaitable{this};
        }

    private:
        struct steady_timer_awaitable {
            constexpr bool await_ready() const noexcept {
                return false;
            }
            constexpr void await_resume() const noexcept {
            }
            template<typename Promise>
            void await_suspend(std::coroutine_handle<Promise> h) noexcept {
                self_->ctx_.call_later(self_->when_, h.promise());
            }
            steady_timer* self_;
        };

    private:
        io_context& ctx_;
        std::chrono::milliseconds when_;
    };
}
