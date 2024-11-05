
#include <chrono>
#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <aifs/event_loop.h>
#include <aifs/spawn.h>
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
        http_server(aifs::io_context& ctx)
            : acceptor_(ctx, 8080)
        {}

        aifs::task<> start()
        {
            try {
                for (;;) {
                    aifs::tcp_socket socket = co_await acceptor_.async_accept();
                    aifs::spawn(handle_connection(std::move(socket)));
                }
            } catch (const std::exception& e) {
                spdlog::error("Got exception while handling connection: {}", e.what());
            }
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
                spdlog::error("Got exception: {}", e.what());
            }
        }

    private:
        aifs::tcp_acceptor acceptor_;
    };
}

int main()
{
    spdlog::info("http_server");
    try {
        aifs::io_context ctx;

        http::http_server server{ctx};
        aifs::spawn(server.start());

        aifs::spawn([&]() -> aifs::task<> {
            co_await aifs::steady_timer{ctx, 10s};
            server.stop();
        }());

        ctx.run();
    } catch (const std::exception &e) {
        spdlog::error("Got exception: {}\n", e.what());
    }
    return 0;
}
