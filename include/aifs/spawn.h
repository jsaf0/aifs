#pragma once

#include "task.h"

namespace aifs {
    namespace detail {
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

    inline void spawn(task<> t)
    {
        [](task<> t) -> detail::oneway_task {
            co_await std::move(t);
        }(std::move(t));
    }
}
