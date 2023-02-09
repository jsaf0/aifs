#pragma once

#include "awaitable.h"
#include "event_loop.h"
#include "task.h"

namespace aifs {
    class timer {
    public:
        virtual ~timer() = default;
        [[nodiscard]] virtual awaitable<> async_wait() = 0;
    };

    class steady_timer : public timer {
    private:
        struct timer_op;

    public:
        steady_timer(io_context& ctx, std::chrono::milliseconds when)
            : ctx_{ctx}, when_{std::move(when)} {
        }

        [[nodiscard]] awaitable<> async_wait() {
            return awaitable<>{timer_op{this}};
        }

        auto operator co_await () noexcept {
            return async_wait();
        }

    private:
        struct timer_op : operation {
            timer_op(steady_timer* self)
                : self_{self}, continuation_{nullptr} {}

            void await_resume() const noexcept {
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
