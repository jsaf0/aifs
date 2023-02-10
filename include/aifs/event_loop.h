#pragma once

#include <chrono>
#include <vector>
#include <queue>
#include <algorithm>
#include <optional>
#include <thread>

#include <spdlog/spdlog.h>

#include "task.h"
#include "non_copyable.h"
#include "operation.h"
#include "descriptor.h"

namespace aifs {
    class event_loop final : private non_copyable {
    public:
        using ms_duration = std::chrono::milliseconds;
        using timer_handle = std::pair<ms_duration, operation*>;
        enum op_type { READ_OP = 0, WRITE_OP = 1, PRI_OP = 2, MAX_OP = 3 };

    public:
        event_loop();
        ~event_loop();

        /**
         * Run the event loop until there are no more work to do.
         */
        void run()
        {
            while (!is_stop()) {
                run_once();
            }
            spdlog::info("run done");
        }

        void stop()
        {
            spdlog::info("Stop loop");
            stopped_ = true;
        }

        void call_later(ms_duration when, operation* op)
        {
            // Add the handle to the schedule, and the sort the schedule to have to earliest
            // expiring timer as the first entry.
            schedule_.emplace_back(time() + when, op);
            std::ranges::push_heap(schedule_, std::ranges::greater{}, &timer_handle::first);
            work_started();
        }

        ms_duration time()
        {
            auto now = std::chrono::steady_clock::now();
            return duration_cast<ms_duration>(now.time_since_epoch()) - start_time_;
        }

        void add_operation(descriptor* desc, op_type type, operation* op)
        {
            pending_ops_[type].emplace_back(desc, op);
            work_started();
        }

        void spawn(task<> t);

    private:
        bool is_stop();
        void run_once();

        void work_started()
        {
            ++outstanding_work_;
        }

        void work_finished()
        {
            if (--outstanding_work_ <= 0) {
                spdlog::info("No more work, stopping");
                stop();
            }
        }

    private:
        bool stopped_;
        std::chrono::milliseconds start_time_;
        std::vector<timer_handle> schedule_;
        std::queue<operation*> ready_;
        std::vector<std::pair<descriptor*, operation*>> pending_ops_[MAX_OP];
        uint64_t outstanding_work_;
    };
}
