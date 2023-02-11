#pragma once

#include <map>
#include <string>

#include <http_parser.h>

#include "aifs/event_loop.h"
#include "aifs/log.h"
#include "aifs/task.h"
#include "aifs/tcp_socket.h"
#include "aifs/steady_timer.h"

namespace aifs::http {
    class req_parser {
    public:
        req_parser()
        {
            parser_settings_.on_message_complete = [](http_parser* p) {
                req_parser* self = reinterpret_cast<req_parser*>(p->data);
                self->complete_ = true;
                return 0;
            };

            parser_settings_.on_url = [](http_parser* p, const char* at, std::size_t n) {
                req_parser* self = reinterpret_cast<req_parser*>(p->data);
                self->url_ = std::string(at, n);
                return 0;
            };

            parser_settings_.on_body = [](http_parser* p, const char* at, std::size_t n) {
                req_parser* self = reinterpret_cast<req_parser*>(p->data);
                self->body_.append(at, n);
                return 0;
            };

            parser_settings_.on_header_field = [](http_parser* p, const char* at, std::size_t n) {
                // spdlog::info("header field: {}", std::string(at, n));
                req_parser* self = reinterpret_cast<req_parser*>(p->data);
                self->cur_header_field_ = std::string(at, n);
                return 0;
            };

            parser_settings_.on_header_value = [](http_parser* p, const char* at, std::size_t n) {
                // spdlog::info("header value: {}", std::string(at, n));
                req_parser* self = reinterpret_cast<req_parser*>(p->data);
                self->headers_.emplace(self->cur_header_field_, std::string(at, n));
                return 0;
            };

            http_parser_init(&parser_, HTTP_REQUEST);
            parser_.data = this;
        }

    public:
        unsigned int consume(std::span<const char> chunk)
        {
            http_parser_execute(&parser_, &parser_settings_, chunk.data(), chunk.size());
            return parser_.http_errno;
        }

    public:
        bool complete_{false};
        std::string url_{};
        std::string body_{};
        std::map<std::string, std::string> headers_{};

    private:
        http_parser_settings parser_settings_{};
        http_parser parser_{};
        std::string cur_header_field_{};
    };

    class http_connection {
    public:
        http_connection(event_loop& ev, std::unique_ptr<tcp_socket> socket)
            : log_{make_logger("conn:" + std::to_string(socket->remote_port()))}
            , ev_{ev}
            , socket_{std::move(socket)}
            , parser_{}
        {
        }

        task<> handle()
        {
            try {
                std::array<char, 8 * 1024> buffer{};
                for (;;) {
                    ssize_t received = co_await socket_->receive(buffer);
                    auto res = parser_.consume({buffer.data(), static_cast<std::size_t>(received)});
                    if (res != HPE_OK && res != HPE_PAUSED) {
                        log_->warn("parsing failed");
                        break;
                    }
                    if (parser_.complete_) {
                        log_->info("Got request for {}", parser_.url_);
                        for (const auto& [f, v] : parser_.headers_) {
                            log_->info("Header: {} = {}", f, v);
                        }
                        log_->info("Body: {}", parser_.body_);
                        break;
                    }
                }

                log_->warn("Reply with 200 OK");
                co_await socket_->send("HTTP/1.1 200 OK\r\n\r\n");
                socket_->close();
            } catch (const std::exception& e) {
                log_->error("Exception: {}", e.what());
            }
        }

    private:
        logger log_;
        event_loop& ev_;
        std::unique_ptr<tcp_socket> socket_;
        req_parser parser_;
    };
}