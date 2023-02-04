
#include "aifs/event_loop.h"
#include "aifs/handle.h"

namespace aifs {
event_loop::event_loop() {
    auto now = std::chrono::steady_clock::now();
    start_time_ = duration_cast<ms_duration>(now.time_since_epoch());
}

std::uint64_t handle::g_handle_id_ = 0;
}

