#pragma once

#include <chrono>
#include <vector>
#include <queue>
#include <algorithm>
#include <optional>
#include <thread>

#include <fmt/core.h>
#include <sys/epoll.h>

#include "non_copyable.h"
#include "handle.h"
#include "operation.h"
#include "descriptor.h"

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

        void add_descriptor(descriptor* desc) {
            fmt::print("add descriptor {}\n", desc->fd_);
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLPRI;
            ev.data.ptr = desc;
            auto ret = epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, desc->fd_, &ev);
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "epoll_ctl");
            }
        }

        void add_op(descriptor* desc, int event, operation* op) {
            desc->ops_[event].push(op);
            fmt::print("add op ---\n");
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

            epoll_event events[128];
            int num_events = 0; //epoll_wait(epoll_fd_, events, 128, timeout.has_value() ? timeout.value().count() : -1);

            std::queue<operation*> ready;
            // Generate queue of ready operations.
            for (int i = 0; i < num_events; ++i) {
                descriptor* desc = static_cast<descriptor *>(events[i].data.ptr);
                for (int j = 0; j < 4; ++j) {
                    if (j & events[i].events && !desc->ops_[j].empty()) {
                        while (operation* op = desc->ops_[j].front()) {
                            ready.push(op);
                            desc->ops_[j].pop();
                        }
                    }
                }
            }


            // Add all expired timers to the ready queue
            auto now = time();
            while (!schedule_.empty()) {
                auto&& [when, info] = schedule_[0];
                if (when > now) {
                    break;
                }
                ready.push(info.h);
                std::ranges::pop_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
                schedule_.pop_back();
            }


            // Run all ready handles
            for (std::size_t todo = ready.size(), i = 0; i < todo; ++i) {
                auto op = ready.front();
                ready.pop();
                op->perform();
            }

//            // Perform the ready options
//            if (!ready.empty()) {
//                while (operation *op = ready.front()) {
//                    ready.pop();
//                    op->perform();
//                }
//            }
        }

    private:
        selector selector_;
        std::chrono::milliseconds start_time_;
        std::vector<timer_handle> schedule_;
        // std::queue<handle_info> ready_;

        int epoll_fd_;
    };

    // Asio-like naming
    using io_context = event_loop;
}
