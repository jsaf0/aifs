
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
        http_connection(aifs::event_loop&, aifs::tcp_socket s)
            : log_{spdlog::stdout_color_mt("conn:" + std::to_string(s.remote_port()))},/* ev_{ev}, */socket_{std::move(s)} {}

        aifs::task<> handle()
        {
            log_->info("Handle connection from {}:{}", socket_.remote_address(), socket_.remote_port());

            std::array<char, 1024> req{};
            int n = co_await socket_.async_receive(req);
            log_->info("Received {} bytes", n);

            const std::string resp = "HTTP/1.1 200 OK\r\n\r\n";
            n = co_await socket_.async_send(resp);
            log_->info("Sent {} bytes", n);

            socket_.close();
            log_->info("Close");
        }

    private:
        std::shared_ptr<spdlog::logger> log_;
        // aifs::event_loop& ev_;
        aifs::tcp_socket socket_;
    };

    class http_server {
    public:
        explicit http_server(aifs::event_loop& ev)
            : ev_{ev}
            , acceptor_(ev, 8080)
        {}

        aifs::task<> start()
        {
            try {
                for (;;) {
                    auto socket = co_await acceptor_.async_accept();
                    ev_.spawn(handle_connection(std::move(socket)));
                }
            } catch (const std::system_error& e) {
                spdlog::error("Got system error: {}", e.what());
            }
        }

        void stop()
        {
            acceptor_.cancel();
        }

    private:
        aifs::task<> handle_connection(aifs::tcp_socket socket)
        {
            try {
                http_connection conn{ev_, std::move(socket)};
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
    spdlog::info("Running HTTP server");
    try {
        aifs::event_loop ev;

        http::http_server server{ev};
        ev.spawn(server.start());

        ev.spawn([&]() -> aifs::task<> {
            co_await aifs::steady_timer{ev, 5s};
            spdlog::info("Timer expired");
            server.stop();
        }());

        ev.run();
    } catch (const std::exception &e) {
        spdlog::error("Got exception ({}): {}\n", __func__, e.what());
    }
    return 0;
}
