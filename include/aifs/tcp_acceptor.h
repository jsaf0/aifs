#pragma once

#include <coroutine>
#include <system_error>

#include <fmt/core.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "awaitable.h"
#include "event_loop.h"
#include "operation.h"
#include "descriptor.h"
#include "tcp_socket.h"

namespace aifs {
    template <typename Socket>
    class acceptor {
    public:
        using socket_ptr = std::unique_ptr<Socket>;
    public:
        virtual ~acceptor() = default;
        virtual void cancel() = 0;
        virtual awaitable<socket_ptr> accept() = 0;
    };

    class tcp_acceptor : public acceptor<tcp_socket> {
    private:
        struct accept_op;

    public:
        tcp_acceptor(event_loop &ctx, int port) : ev_{ctx}, desc_{-1}
        {
            desc_.fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (desc_.fd < 0) {
                throw std::system_error(errno, std::generic_category(), "socket");
            }

            int ret = ::fcntl(desc_.fd, F_SETFL, O_NONBLOCK);
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "fcntl");
            }

            int val = 1;
            ret = ::setsockopt(desc_.fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "setsockopt");
            }

            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);
            ret = ::bind(desc_.fd, (sockaddr*)&addr, sizeof(addr));
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "bind");
            }

            ret = ::listen(desc_.fd, SOMAXCONN);
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "listen");
            }
        }

        ~tcp_acceptor() override
        {
            if (desc_.fd != -1) {
                ::close(desc_.fd);
            }
        }

        void cancel() override
        {
            ev_.cancel_operation(&desc_, event_loop::op_type::read_op);
        }

        [[nodiscard]] awaitable<socket_ptr> accept() override
        {
            if (desc_.fd == -1) {
                throw std::runtime_error("Not ready to accept");
            }
            return awaitable<socket_ptr>{accept_op{this}};
        }

    private:
        struct accept_op : operation {
            explicit accept_op(tcp_acceptor* acceptor) : acceptor_{acceptor}
            { }

            socket_ptr await_resume()
            {
                if (auto ec = std::get_if<std::error_code>(&result_)) {
                    throw std::system_error(*ec);
                }
                return std::move(std::get<socket_ptr>(result_));
            }

            void await_suspend(std::coroutine_handle<> h)
            {
                waiter_ = h;
                acceptor_->ev_.add_operation(&acceptor_->desc_, event_loop::op_type::read_op, this);
            }

            void perform(const std::error_code& ec) override
            {
                if (ec) {
                    result_ = ec;
                    waiter_.resume();
                    return;
                }

                sockaddr_in client_addr{0};
                auto len = sizeof(client_addr);
                auto fd = ::accept(acceptor_->desc_.fd, reinterpret_cast<sockaddr *>(&client_addr),
                                   reinterpret_cast<socklen_t *>(&len));
                if (fd < 0) {
                    result_ = std::make_error_code(static_cast<std::errc>(errno));
                } else {
                    result_.emplace<socket_ptr>(std::make_unique<tcp_socket>(acceptor_->ev_, fd, client_addr));
                }
                waiter_.resume();
            }

            tcp_acceptor* acceptor_;
            std::variant<std::monostate, std::error_code, socket_ptr> result_;
            std::coroutine_handle<> waiter_;
        };

        event_loop &ev_;
        descriptor desc_;
    };
}
