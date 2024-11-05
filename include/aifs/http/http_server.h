#pragma once

#include <spdlog/spdlog.h>

#include "aifs/event_loop.h"
#include "aifs/task.h"
#include "aifs/tcp_acceptor.h"
#include "aifs/http/http_connection.h"

namespace aifs::http {
    class http_server {
    public:
        explicit http_server(aifs::event_loop& ev, aifs::acceptor<tcp_socket>& acceptor)
                : ev_{ev}
                , acceptor_{acceptor}
        {}

        aifs::task<> start()
        {
            try {
                for (;;) {
                    auto socket = co_await acceptor_.accept();
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
        aifs::task<> handle_connection(aifs::acceptor<tcp_socket>::socket_ptr socket)
        {
            try {
                aifs::http::http_connection conn{ev_, std::move(socket)};
                co_await conn.handle();
            } catch (const std::exception& e) {
                spdlog::error("Got exception ({}): {}", __func__, e.what());
            }
        }

    private:
        aifs::event_loop& ev_;
        aifs::acceptor<tcp_socket>& acceptor_;
    };
}