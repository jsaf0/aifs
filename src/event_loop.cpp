
#include "aifs/event_loop.h"

#include <sys/select.h>

namespace aifs {
namespace detail {
    template <typename Duration>
    void to_timeval(Duration&& d, struct timeval& tv)
    {
        std::chrono::seconds const sec = std::chrono::duration_cast<std::chrono::seconds>(d);
        tv.tv_sec = sec.count();
        tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - sec).count();
    }

    struct oneway_task {
        struct promise_type {
            std::suspend_never initial_suspend() const noexcept { return {}; }
            std::suspend_never final_suspend() const noexcept { return {}; }
            void unhandled_exception() { std::terminate(); }
            oneway_task get_return_object() { return {}; }
            void return_void() { }
        };
    };
} // namespace detail

EventLoop::EventLoop()
    : m_stopped { false }
    , m_startTime {}
    , m_outstandingWork { 0 }
{
    auto now = std::chrono::steady_clock::now();
    m_startTime = duration_cast<MSDuration>(now.time_since_epoch());
}

EventLoop::~EventLoop() = default;

bool EventLoop::isStop() const { return m_stopped; }

void EventLoop::spawn(Task<> t)
{
    [](Task<> t, EventLoop* self) -> detail::oneway_task {
        self->workStarted();
        co_await std::move(t);
        self->workFinished();
    }(std::move(t), this);
}

void EventLoop::runOnce()
{
    // If we have any waiting timers, the max timeout of the select Operation
    // should be the time until the first Timer expires.
    std::optional<MSDuration> timeout;
    if (!m_schedule.empty()) {
        auto&& [when, _] = m_schedule[0];
        timeout = std::max(when - time(), std::chrono::milliseconds { 0 });
    }

    // Build the fd_sets based on pending operations.
    int max_fd = -1;
    fd_set fd_sets[max_op];
    for (int i = 0; i < max_op; ++i) {
        FD_ZERO(&fd_sets[i]);
        for (const auto& [desc, op] : m_pendingOps[i]) {
            FD_SET(desc->m_fd, &fd_sets[i]);
            if (desc->m_fd > max_fd) {
                max_fd = desc->m_fd;
            }
        }
    }

    struct timeval to = { 0, 0 };
    struct timeval* tv = nullptr;
    if (timeout) {
        detail::to_timeval(*timeout, to);
        tv = &to;
    }

    int num_events = ::select(max_fd + 1, &fd_sets[0], &fd_sets[1], &fd_sets[2], tv);
    if (num_events > 0) {
        // Add all ready operations to the ready queue and remove from queue of
        // pending operations
        for (int i = 0; i < max_op; ++i) {
            auto it = m_pendingOps[i].begin();
            while (it != m_pendingOps[i].end()) {
                if (FD_ISSET(it->first->m_fd, &fd_sets[i]) != 0) {
                    m_ready.push(it->second);
                    it = m_pendingOps[i].erase(it);
                } else {
                    it++;
                }
            }
        }
    }

    // Add all expired timers to the m_ready queue
    auto now = time();
    while (!m_schedule.empty()) {
        auto&& [when, op] = m_schedule[0];
        if (when > now) {
            break;
        }
        m_ready.push(op);
        std::ranges::pop_heap(m_schedule, std::ranges::greater {}, &TimerHandle::first);
        m_schedule.pop_back();
    }

    // Run all m_ready handles
    while (!m_ready.empty()) {
        auto op = m_ready.front();
        m_ready.pop();
        op->perform(op->ec);
        workFinished();
    }
}
} // namespace aifs
