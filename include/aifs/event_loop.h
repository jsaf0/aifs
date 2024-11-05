#pragma once

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <queue>
#include <thread>
#include <vector>

#include "descriptor.h"
#include "non_copyable.h"
#include "operation.h"
#include "task.h"

namespace aifs {
class EventLoop final : private NonCopyable {
public:
    using MSDuration = std::chrono::milliseconds;
    using TimerHandle = std::pair<MSDuration, Operation*>;
    enum OpType { read_op = 0,
        write_op = 1,
        pri_op = 2,
        max_op = 3 };

public:
    EventLoop();
    ~EventLoop();

    /**
     * Run the event loop until there are no more work to do.
     */
    void run()
    {
        if (m_outstandingWork == 0) {
            return;
        }

        while (!isStop()) {
            runOnce();
        }
    }

    void stop()
    {
        spdlog::info("Stop loop");
        m_stopped = true;
    }

    void callLater(MSDuration when, Operation* op)
    {
        // Add the handle to the schedule, and the sort the schedule to have to
        // earliest expiring Timer as the first entry.
        m_schedule.emplace_back(time() + when, op);
        std::ranges::push_heap(m_schedule, std::ranges::greater {}, &TimerHandle::first);
        workStarted();
    }

    MSDuration time()
    {
        auto now = std::chrono::steady_clock::now();
        return duration_cast<MSDuration>(now.time_since_epoch()) - m_startTime;
    }

    void addOperation(Descriptor* desc, OpType type, Operation* op)
    {
        m_pendingOps[type].emplace_back(desc, op);
        workStarted();
    }

    void cancelOperation(Descriptor* desc, OpType type)
    {
        for (auto it = m_pendingOps[type].begin(); it != m_pendingOps[type].end();) {
            if (it->first == desc) {
                it->second->ec = std::make_error_code(std::errc::operation_canceled);
                m_ready.push(it->second);
                m_pendingOps[type].erase(it);
                break;
            }
        }
    }

    void spawn(Task<> t);

private:
    [[nodiscard]] bool isStop() const;
    void runOnce();

    void workStarted() { ++m_outstandingWork; }

    void workFinished()
    {
        if (--m_outstandingWork <= 0) {
            spdlog::info("No more work, stopping");
            stop();
        }
    }

private:
    bool m_stopped;
    std::chrono::milliseconds m_startTime;
    std::vector<TimerHandle> m_schedule;
    std::queue<Operation*> m_ready;
    std::vector<std::pair<Descriptor*, Operation*>> m_pendingOps[max_op];
    uint64_t m_outstandingWork;
};
} // namespace aifs
