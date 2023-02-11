#pragma once

#include <http_parser.h>

#include <map>
#include <string>

#include "aifs/event_loop.h"
#include "aifs/log.h"
#include "aifs/steady_timer.h"
#include "aifs/task.h"
#include "aifs/tcp_socket.h"

namespace aifs::http {
class RequestParser {
public:
    RequestParser()
    {
        m_parser_settings.on_message_complete = [](http_parser* p) {
            RequestParser* self = reinterpret_cast<RequestParser*>(p->data);
            self->m_complete = true;
            return 0;
        };

        m_parser_settings.on_url = [](http_parser* p, const char* at, std::size_t n) {
            RequestParser* self = reinterpret_cast<RequestParser*>(p->data);
            self->m_url = std::string(at, n);
            return 0;
        };

        m_parser_settings.on_body = [](http_parser* p, const char* at, std::size_t n) {
            RequestParser* self = reinterpret_cast<RequestParser*>(p->data);
            self->m_body.append(at, n);
            return 0;
        };

        m_parser_settings.on_header_field = [](http_parser* p, const char* at, std::size_t n) {
            RequestParser* self = reinterpret_cast<RequestParser*>(p->data);
            self->m_cur_header_field = std::string(at, n);
            return 0;
        };

        m_parser_settings.on_header_value = [](http_parser* p, const char* at, std::size_t n) {
            RequestParser* self = reinterpret_cast<RequestParser*>(p->data);
            self->m_headers.emplace(self->m_cur_header_field, std::string(at, n));
            return 0;
        };

        http_parser_init(&m_parser, HTTP_REQUEST);
        m_parser.data = this;
    }

public:
    unsigned int consume(std::span<const char> chunk)
    {
        http_parser_execute(&m_parser, &m_parser_settings, chunk.data(), chunk.size());
        return m_parser.http_errno;
    }

public:
    bool m_complete { false };
    std::string m_url {};
    std::string m_body {};
    std::map<std::string, std::string> m_headers {};

private:
    http_parser_settings m_parser_settings {};
    http_parser m_parser {};
    std::string m_cur_header_field {};
};

class HTTPConnection {
public:
    HTTPConnection(EventLoop& ev, std::unique_ptr<TCPSocket> socket)
        : m_log { makeLogger("conn:" + std::to_string(socket->remote_port())) }
        , m_eventLoop { ev }
        , m_socket { std::move(socket) }
        , m_parser {}
    {
    }

    Task<> handle()
    {
        try {
            std::array<char, 8 * 1024> buffer {};
            for (;;) {
                ssize_t received = co_await m_socket->receive(buffer);
                auto res = m_parser.consume({ buffer.data(), static_cast<std::size_t>(received) });
                if (res != HPE_OK && res != HPE_PAUSED) {
                    m_log->warn("parsing failed");
                    break;
                }
                if (m_parser.m_complete) {
                    m_log->info("Got request for {}", m_parser.m_url);
                    for (const auto& [f, v] : m_parser.m_headers) {
                        m_log->info("Header: {} = {}", f, v);
                    }
                    m_log->info("Body: {}", m_parser.m_body);
                    break;
                }
            }

            m_log->warn("Reply with 200 OK");
            co_await m_socket->send("HTTP/1.1 200 OK\r\n\r\n");
            m_socket->close();
        } catch (const std::exception& e) {
            m_log->error("Exception: {}", e.what());
        }
    }

private:
    Logger m_log;
    EventLoop& m_eventLoop;
    std::unique_ptr<TCPSocket> m_socket;
    RequestParser m_parser;
};
} // namespace aifs::http