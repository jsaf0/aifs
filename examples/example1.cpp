
#include <chrono>

#include <fmt/core.h>

#include <aifs/event_loop.h>
#include <aifs/task.h>
#include <aifs/steady_timer.h>

using namespace std::chrono_literals;

auto returns_custom_awaitable() {
    struct awaitable {
        constexpr bool await_ready() const noexcept {
            return true;
        }
        constexpr int await_resume() const noexcept {
            return n_;
        }
        void await_suspend(std::coroutine_handle<>) noexcept {
        }
        int n_;
    };
    return awaitable{123};
}

aifs::awaitable<int> get_async(aifs::io_context& ctx) {
    aifs::steady_timer t{ctx, 10ms};
    co_await t.async_wait();
    co_return 123;
}

aifs::awaitable<void> empty(aifs::io_context& ctx) {
    fmt::print("{} called\n", __func__);
    fmt::print("{} done\n", __func__);
    co_return;
}

aifs::awaitable<void> slow(aifs::io_context& ctx) {
    fmt::print("{} called\n", __func__);
    aifs::steady_timer t{ctx, 5ms};
    co_await t.async_wait();
    fmt::print("{} done\n", __func__);
    co_return;
}

aifs::awaitable<void> listener(aifs::io_context& ctx) {
    fmt::print("listener() called\n");
    co_await slow(ctx);
    co_await empty(ctx);
    auto n = co_await returns_custom_awaitable();
    fmt::print("n = {}\n", n);
    auto v = co_await get_async(ctx);
    fmt::print("listener() return (v = {})\n", v);
    co_return;
}

int main() {
    try {
        aifs::io_context ctx;

        // TODO: Simple way of spawning a task deferred (instead of co_spawn-like interface)
        auto listener_task = listener(ctx);
        ctx.call_later(0ms, listener_task.handle_.promise());

        ctx.run();
        fmt::print("Done\n");
    } catch (const std::exception& e) {
        fmt::print("Exception: {}\n", e.what());
    }
    return 0;
}
