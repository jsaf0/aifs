
#include "aifs/event_loop.h"

#include <sys/select.h>

namespace aifs {
    namespace detail {
        template<typename Duration>
        void to_timeval(Duration&& d, struct timeval & tv) {
            std::chrono::seconds const sec = std::chrono::duration_cast<std::chrono::seconds>(d);
            tv.tv_sec  = sec.count();
            tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(d - sec).count();
        }

        struct oneway_task {
            struct promise_type {
                std::suspend_never initial_suspend() const noexcept { return {}; }
                std::suspend_never final_suspend() const noexcept { return {}; }
                void unhandled_exception() { std::terminate(); }
                oneway_task get_return_object() { return {}; }
                void return_void() {}
            };
        };
    }

    event_loop::event_loop()
        : stopped_{false}
        , start_time_{}
        , outstanding_work_{0}
    {
        auto now = std::chrono::steady_clock::now();
        start_time_ = duration_cast<ms_duration>(now.time_since_epoch());
    }

    event_loop::~event_loop() = default;

    bool event_loop::is_stop() const
    {
        return stopped_;
    }

    void event_loop::spawn(task<> t)
    {
        [](task<> t, event_loop* self) -> detail::oneway_task {
            self->work_started();
            co_await std::move(t);
            self->work_finished();
        }(std::move(t), this);
    }

    void event_loop::run_once()
    {
        // If we have any waiting timers, the max timeout of the select operation
        // should be the time until the first timer expires.
        std::optional<ms_duration> timeout;
        if (!schedule_.empty()) {
            auto&& [when, _] = schedule_[0];
            timeout = std::max(when - time(), std::chrono::milliseconds{0});
        }

        // Build the fd_sets based on pending operations.
        int max_fd = -1;
        fd_set fd_sets[max_op];
        for (int i = 0; i < max_op; ++i) {
            FD_ZERO(&fd_sets[i]);
            for (const auto& [desc, op] : pending_ops_[i]) {
                FD_SET(desc->fd, &fd_sets[i]);
                if (desc->fd > max_fd) {
                    max_fd = desc->fd;
                }
            }
        }

        struct timeval to = { 0, 0 };
        struct timeval *tv = nullptr;
        if (timeout) {
            detail::to_timeval(*timeout, to);
            tv = &to;
        }

        int num_events = ::select(max_fd + 1, &fd_sets[0], &fd_sets[1], &fd_sets[2], tv);
        if (num_events > 0) {
            // Add all ready operations to the ready queue and remove from queue of pending operations
            for (int i = 0; i < max_op; ++i) {
                auto it = pending_ops_[i].begin();
                while (it != pending_ops_[i].end()) {
                    if (FD_ISSET(it->first->fd, &fd_sets[i]) != 0) {
                        ready_.push(it->second);
                        it = pending_ops_[i].erase(it);
                    } else {
                        it++;
                    }
                }
            }
        }

        // Add all expired timers to the ready_ queue
        auto now = time();
        while (!schedule_.empty()) {
            auto&& [when, op] = schedule_[0];
            if (when > now) {
                break;
            }
            ready_.push(op);
            std::ranges::pop_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
            schedule_.pop_back();
        }

        // Run all ready_ handles
        while (!ready_.empty()) {
            auto op = ready_.front();
            ready_.pop();
            op->perform(op->ec);
            work_finished();
        }
    }
}
