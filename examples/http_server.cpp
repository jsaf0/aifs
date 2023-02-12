
#include <spdlog/spdlog.h>

#include "aifs/http/express_router.h"
#include <aifs/event_loop.h>
#include <aifs/http/http_server.h>
#include <aifs/task.h>
#include <aifs/tcp_acceptor.h>

using namespace aifs;
using namespace aifs::http;
using namespace std::chrono_literals;

template <typename T>
struct ImmediateResult {
    T await_resume() { return value; }
    void await_suspend(std::coroutine_handle<> h) { h.resume(); }
    T value;
};

class FakeTCPSocket : public StreamSocket {
public:
    FakeTCPSocket() { spdlog::info("Fake TCP socket"); }
    virtual unsigned short remote_port() const { return 0; }
    virtual Awaitable<ssize_t> receive(std::span<char> buffer)
    {
        return Awaitable<ssize_t> { ImmediateResult<ssize_t>{0} };
    }
    virtual Awaitable<ssize_t> send(std::span<const char> buffer)
    {
        return Awaitable<ssize_t> { ImmediateResult<ssize_t>{0} };
    }
    virtual void close() const { }
};

int main()
{
    spdlog::info("Running HTTP server");
    try {
        EventLoop ev;

        ExpressRouter router {};
        router.get("/hello", [](auto, auto resp) -> Task<HandlerStatus> {
            spdlog::info("'/hello' handler called");
            resp.setStatus(200);
            resp.setHeader("Content-Type", "text/plain");
            co_await resp.send("Hello, you!");
            co_return HandlerStatus::Accepted;
        });

        router.get("/foo", [](auto, auto resp) -> Task<HandlerStatus> {
            spdlog::info("'/foo' handler called");
            resp.setStatus(200);
            resp.setHeader("Content-Type", "text/plain");
            co_await resp.send("Hello, foooooo");
            co_return HandlerStatus::Accepted;
        });

#if 0
        ev.spawn([]() -> Task<> {
            FakeTCPSocket s {};
            Response r { s };
            r.setStatus(200);
            co_await r.send("Hello, world");
        }());
#endif

        TCPAcceptor acceptor { ev, 8080 };
        HTTPServer server { ev, acceptor, router };
        ev.spawn(server.start());

        ev.run();
    } catch (const std::exception& e) {
        spdlog::error("Got exception ({}): {}\n", __func__, e.what());
    }
    return 0;
}
