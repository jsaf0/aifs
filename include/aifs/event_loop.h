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
        using timer_handle = std::pair<ms_duration, operation*>;
        enum op_type { READ_OP = 0, WRITE_OP = 1, PRI_OP = 2, MAX_OP = 3 };

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
         */
        void call_later(ms_duration when, operation* op) {
            // Add the handle to the schedule, and the sort the schedule to have to earliest
            // expiring timer as the first entry.
            schedule_.emplace_back(time() + when, op);
            std::ranges::push_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
        }

        void dispatch(operation* op) {
            throw std::runtime_error("Not implemented");
        }

        void cancel_delayed_op(operation* op) {
            throw std::runtime_error("Not implemented");
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

        void add_op(descriptor* desc, op_type t, operation* op) {
            if (desc->ops_[t] != nullptr) {
                throw std::runtime_error("An operation is already registered for this event\n");
            }
            desc->ops_[t] = op;
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

            epoll_event events[128];
            int num_events = epoll_wait(epoll_fd_, events, 128, timeout.has_value() ? timeout.value().count() : -1);

            // Translation between op_type and epoll event type.
            static const int epoll_flag[] = { EPOLLIN, EPOLLOUT, EPOLLPRI };

            // Generate queue of ready_ operations.
            for (int i = 0; i < num_events; ++i) {
                descriptor* desc = static_cast<descriptor *>(events[i].data.ptr);
                for (int j = 0; j < op_type::MAX_OP; ++j) {
                    if (desc->ops_[j] && events[i].events & (epoll_flag[j] | EPOLLERR | EPOLLHUP)) {
                        ready_.push(desc->ops_[j]);
                        desc->ops_[j] = nullptr;
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
                op->perform();
            }
        }

    private:
        selector selector_;
        std::chrono::milliseconds start_time_;
        std::vector<timer_handle> schedule_;
        std::queue<operation*> ready_;
        int epoll_fd_;
    };

    // Asio-like naming
    using io_context = event_loop;
}
