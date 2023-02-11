#pragma once

#include <spdlog/spdlog.h>

#include "aifs/event_loop.h"
#include "aifs/http/http_connection.h"
#include "aifs/task.h"
#include "aifs/tcp_acceptor.h"

namespace aifs::http {
class HTTPServer {
public:
    explicit HTTPServer(EventLoop& ev, Acceptor<TCPSocket>& acceptor)
        : m_eventLoop { ev }
        , m_acceptor { acceptor }
    {
    }

    Task<> start()
    {
        try {
            for (;;) {
                auto socket = co_await m_acceptor.accept();
                m_eventLoop.spawn(handleConnection(std::move(socket)));
            }
        } catch (const std::system_error& e) {
            spdlog::error("Got system error: {}", e.what());
        }
    }

    void stop() { m_acceptor.cancel(); }

private:
    Task<> handleConnection(Acceptor<TCPSocket>::SocketPtr socket)
    {
        try {
            HTTPConnection conn { m_eventLoop, std::move(socket) };
            co_await conn.handle();
        } catch (const std::exception& e) {
            spdlog::error("Got exception ({}): {}", __func__, e.what());
        }
    }

private:
    EventLoop& m_eventLoop;
    Acceptor<TCPSocket>& m_acceptor;
};
} // namespace aifs::http