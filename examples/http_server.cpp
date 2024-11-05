
#include <spdlog/spdlog.h>

#include "aifs/http/express_router.h"
#include <aifs/event_loop.h>
#include <aifs/http/http_server.h>
#include <aifs/task.h>
#include <aifs/tcp_acceptor.h>

using namespace aifs;
using namespace aifs::http;
using namespace std::chrono_literals;

int main()
{
    spdlog::info("Running HTTP server");
    try {
        EventLoop ev;

        ExpressRouter router {};
        router.get("/hello", [](auto, auto resp) -> Task<> {
            spdlog::info("'/hello' handler called");
            resp.setStatus(200);
            resp.setHeader("Content-Type", "text/plain");
            co_await resp.send("Hello, you!");
        });

        router.get("/foo", [](auto, auto resp) -> Task<> {
            spdlog::info("'/foo' handler called");
            resp.setStatus(200);
            resp.setHeader("Content-Type", "text/plain");
            co_await resp.send("Hello, foooooo");
        });

        TCPAcceptor acceptor { ev, 8080 };
        HTTPServer server { ev, acceptor, router };
        ev.spawn(server.start());

        ev.run();
    } catch (const std::exception& e) {
        spdlog::error("Got exception ({}): {}\n", __func__, e.what());
    }
    return 0;
}
