#pragma once

#include <chrono>
#include <vector>
#include <queue>
#include <algorithm>
#include <optional>
#include <thread>

#include <fmt/core.h>

#include "non_copyable.h"
#include "handle.h"

namespace aifs {

#if defined(__APPLE__)
    class kqueue_selector {
    };
    using selector = kqueue_selector;
#elif defined(__linux__)
    class epoll_selector {
    };
    using selector = epoll_selector;
#endif

    class event_loop : private non_copyable {
    public:
        using ms_duration = std::chrono::milliseconds;
        using timer_handle = std::pair<ms_duration, handle_info>;

    public:
        event_loop();

        /**
         * Run the event loop until there are no more work to do.
         */
        void run() {
            while (!is_stop()) {
                run_once();
            }
        }

        /**
         * Add a handle/callback to be called in a later point in time.
         *
         * @param when Offset in time (ms) from now
         * @param handle The handle to run/call
         */
        void call_later(ms_duration when, handle& callback) {
            fmt::print("call handle {} at {}\n", callback.id_, when.count());
            // Add the handle to the schedule, and the sort the schedule to have to earliest
            // expiring timer as the first entry.
            schedule_.emplace_back(time() + when, handle_info{callback.id_, &callback});
            std::ranges::push_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
        }

        /**
         * @return The offset in milliseconds since creating the loop
         */
        ms_duration time() {
            auto now = std::chrono::steady_clock::now();
            return duration_cast<ms_duration>(now.time_since_epoch()) - start_time_;
        }

    private:
        bool is_stop() {
            return schedule_.empty();
        }

        void run_once() {
            // If we have any waiting timers, the max timeout of the select operation
            // should be the time until the first timer expires.
            std::optional<ms_duration> timeout;
            if (!schedule_.empty()) {
                auto&& [when, _] = schedule_[0];
                timeout = std::max(when - time(), std::chrono::milliseconds{0});
            }

            // TODO: Replace with call to selector/reactor
            if (timeout) {
                std::this_thread::sleep_for(timeout.value());
            }

            // Add all expired timers to the ready queue
            auto now = time();
            while (!schedule_.empty()) {
                auto&& [when, info] = schedule_[0];
                if (when > now) {
                    break;
                }
                ready_.push(info);
                std::ranges::pop_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
                schedule_.pop_back();
            }

            // Run all ready handles
            for (std::size_t todo = ready_.size(), i = 0; i < todo; ++i) {
                auto [handle_id, handle] = ready_.front();
                ready_.pop();
                handle->run();
            }
        }

    private:
        selector selector_;
        std::chrono::milliseconds start_time_;
        std::vector<timer_handle> schedule_;
        std::queue<handle_info> ready_;
    };

    // Asio-like naming
    using io_context = event_loop;
}
