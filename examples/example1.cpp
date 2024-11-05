
#include <chrono>

#include <spdlog/spdlog.h>

#include <aifs/event_loop.h>
#include <aifs/http/http_server.h>
#include <aifs/steady_timer.h>
#include <aifs/task.h>
#include <aifs/tcp_acceptor.h>

using namespace std::chrono_literals;

int main()
{
    spdlog::info("Running HTTP server");
    try {
        aifs::EventLoop ev;
        aifs::TCPAcceptor acceptor { ev, 8080 };
        aifs::http::HTTPServer server { ev, acceptor };
        ev.spawn(server.start());

        ev.spawn([&]() -> aifs::Task<> {
            co_await aifs::SteadyTimer { ev, 60s };
            spdlog::info("Timer expired");
            server.stop();
        }());

        ev.run();
    } catch (const std::exception& e) {
        spdlog::error("Got exception ({}): {}\n", __func__, e.what());
    }
    return 0;
}
