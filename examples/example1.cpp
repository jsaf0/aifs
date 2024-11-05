
#include <chrono>

#include <fmt/core.h>

#include <aifs/event_loop.h>
#include <aifs/task.h>
#include <aifs/steady_timer.h>
#include <aifs/tcp_socket.h>
#include <aifs/tcp_acceptor.h>

using namespace std::chrono_literals;

#if 0
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
#endif

#include <coroutine>

struct oneway_task
{
    struct promise_type
    {
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void unhandled_exception() { std::terminate(); }
        oneway_task get_return_object() { return {}; }
        void return_void() {}
    };
};

template <typename A>
void do_spawn(A&& a)
{
    [](std::decay_t<A> a) -> oneway_task {
        // TODO: Do something before and after task runs?
        co_await std::move(a);
    }(std::forward<A>(a));
}

aifs::task<void> hello2(aifs::io_context& ctx, int j, int i)
{
    aifs::steady_timer t{ctx, 2000ms + std::chrono::milliseconds{i * 10}};
    co_await t.async_wait();
    fmt::print("Hello {}:{}\n", j, i);
    co_return;
}


aifs::task<void> hello(aifs::io_context& ctx, int j)
{
    for (int i = 0; i < 5; ++i) {
        do_spawn(hello2(ctx, j, i));
    }
    aifs::steady_timer t{ctx, 1000ms};
    co_await t.async_wait();
    fmt::print("Hello {}\n", j);
    co_return;
}

aifs::task<void> long_sleep(aifs::io_context& ctx)
{
    aifs::steady_timer t{ctx, 5000ms};
    co_await t.async_wait();
}

aifs::task<void> echo(aifs::io_context& ctx, aifs::tcp_socket sock)
{
    aifs::steady_timer t{ctx, 100ms};
    co_await t.async_wait();
    sock.close();

    fmt::print("bye bye conn - {}\n", sock.remote_port());
    co_return;
}

aifs::task<void> listener(aifs::io_context &ctx)
{
    try {
        aifs::tcp_acceptor acceptor{ctx, 22222};
//        for (;;) {
//            fmt::print("-- WAIT FOR NEW CONN! --\n");
            auto s = co_await acceptor.async_accept();
            fmt::print("Got connection from {}:{}\n", s.remote_address(), s.remote_port());
            do_spawn(echo(ctx, std::move(s)));
//        }
    } catch (const std::exception& e) {
        fmt::print("Got exception: {}\n", e.what());
    }
    fmt::print("done listening for connections\n");
    co_return;
}



int main() {
    fmt::print("running example1\n");
    try {
        aifs::io_context ctx;

        // do_spawn(hello(ctx, 1));
        // do_spawn(hello(ctx, 2));
        do_spawn(long_sleep(ctx));
        do_spawn(listener(ctx));

        ctx.run();
        fmt::print("Done\n");
    } catch (const std::exception &e) {
        fmt::print("Got exception: {}\n", e.what());
    }
    return 0;
}
