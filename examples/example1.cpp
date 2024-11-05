
#include <chrono>

#include <fmt/core.h>

#include <aifs/event_loop.h>
#include <aifs/task.h>
#include <aifs/steady_timer.h>
#include <aifs/tcp_socket.h>
#include <aifs/tcp_acceptor.h>

using namespace std::chrono_literals;

template <typename Awaitable>
void spawn(Awaitable&& awaitable)
{
    awaitable.handle_.promise().perform();
}

aifs::task<void> echo(aifs::io_context& ctx, aifs::tcp_socket sock)
{
    aifs::steady_timer t{ctx, 3000ms};
    co_await t.async_wait();
    sock.close();

    fmt::print("bye bye conn - {}\n", sock.remote_port());
    co_return;
}

aifs::task<void> listener(aifs::io_context &ctx)
{
    try {
        aifs::tcp_acceptor acceptor{ctx, 22222};
        for (;;) {
            fmt::print("-- WAIT FOR NEW CONN! --\n");
            auto s = co_await acceptor.async_accept();
            fmt::print("Got connection from {}:{}\n", s.remote_address(), s.remote_port());
            // co_await echo(ctx, std::move(s));

            spawn(echo(ctx, std::move(s)));


        }
    } catch (const std::exception& e) {
        fmt::print("Got exception: {}\n", e.what());
    }
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
    } catch (const std::exception &e) {
        fmt::print("Got exception: {}\n", e.what());
    }
    return 0;
}
