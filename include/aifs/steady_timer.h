#pragma once

#include "awaitable.h"
#include "event_loop.h"
#include "task.h"

namespace aifs {
class Timer {
public:
    virtual ~Timer() = default;
    [[nodiscard]] virtual Awaitable<> wait() = 0;
};

class SteadyTimer : public Timer {
private:
    struct TimerOp;

public:
    SteadyTimer(EventLoop& ctx, std::chrono::milliseconds when)
        : m_eventLoop { ctx }
        , m_when { std::move(when) }
    {
    }

    [[nodiscard]] Awaitable<> wait()
    {
        return Awaitable<> { TimerOp { this } };
    }

    auto operator co_await() noexcept { return wait(); }

private:
    struct TimerOp : Operation {
        TimerOp(SteadyTimer* self)
            : m_self { self }
            , m_continuation { nullptr }
        {
        }

        void await_resume() const noexcept { }

        void await_suspend(std::coroutine_handle<> h) noexcept
        {
            m_continuation = h;
            m_self->m_eventLoop.callLater(m_self->m_when, this);
        }

        void perform(const std::error_code&)
        {
            if (m_continuation) {
                m_continuation.resume();
            }
        }

        SteadyTimer* m_self;
        std::coroutine_handle<> m_continuation;
    };

private:
    EventLoop& m_eventLoop;
    std::chrono::milliseconds m_when;
};
} // namespace aifs
