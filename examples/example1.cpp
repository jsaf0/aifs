
#include <chrono>
#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <aifs/event_loop.h>
#include <aifs/steady_timer.h>
#include <aifs/task.h>
#include <aifs/tcp_acceptor.h>
#include <aifs/tcp_socket.h>

using namespace std::chrono_literals;

namespace http {
    class http_connection {
    public:
        http_connection(aifs::tcp_socket s)
            : socket_{std::move(s)} {}

        aifs::task<> handle()
        {
            spdlog::info("Handle connection from {}:{}", socket_.remote_address(), socket_.remote_port());
            const std::string resp = "HTTP/1.1 200 OK\r\n\r\n";
            /*co_await*/ socket_.send(resp);
            socket_.close();
            co_return;
        }

    private:
        std::shared_ptr<spdlog::logger> log_;
        aifs::tcp_socket socket_;
    };

    class http_server {
    public:
        http_server(aifs::event_loop& ev)
            : ev_{ev}
            , acceptor_(ev, 8080)
        {}

        aifs::task<> start()
        {
            aifs::acceptor<aifs::tcp_socket>& acceptor = acceptor_;
            try {
                // XXX: Stop after handling two connections!
                for (int i = 0; i < 2; ++i) {
                    auto socket = co_await acceptor.async_accept();
                    ev_.spawn(handle_connection(std::move(socket)));
                }
            } catch (const std::exception& e) {
                spdlog::error("Got exception while handling connection: {}", e.what());
            }
            spdlog::warn("Done");
        }

        void stop()
        {
            // TODO: Handle stop - cancel connections and acceptor
            spdlog::info("Stop");
        }

    private:
        aifs::task<> handle_connection(aifs::tcp_socket socket)
        {
            try {
                http_connection conn{std::move(socket)};
                co_await conn.handle();
            } catch (const std::exception& e) {
                spdlog::error("Got exception ({}): {}", __func__, e.what());
            }
        }

    private:
        aifs::event_loop& ev_;
        aifs::tcp_acceptor acceptor_;
    };
}

int main()
{
    spdlog::info("http_server");
    try {
        aifs::event_loop ev;

        http::http_server server{ev};
        ev.spawn(server.start());

        ev.spawn([&]() -> aifs::task<> {
            co_await aifs::steady_timer{ev, 1s};
            spdlog::info("Timer expired");
        }());

        ev.run();
    } catch (const std::exception &e) {
        spdlog::error("Got exception ({}): {}\n", __func__, e.what());
    }
    return 0;
}
