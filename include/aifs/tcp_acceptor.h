#pragma once

#include <coroutine>

#include <fmt/core.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "event_loop.h"
#include "operation.h"
#include "descriptor.h"
#include "tcp_socket.h"

namespace aifs {
    class tcp_acceptor {
    private:
        struct accept_op;

    public:
        tcp_acceptor(event_loop &ctx, int port) : ev_{ctx}, desc_{-1}
        {
            desc_.fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (desc_.fd < 0) {
                throw std::system_error(errno, std::generic_category(), "socket");
            }

            int val = 1;
            int ret = ::setsockopt(desc_.fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "setsockopt");
            }

            sockaddr_in addr;
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

        ~tcp_acceptor()
        {
            if (desc_.fd != -1) {
                ::close(desc_.fd);
            }
        }

        [[nodiscard]] accept_op async_accept()
        {
            if (desc_.fd == -1) {
                throw std::runtime_error("Not ready to accept");
            }
            return {this};
        }

    private:
        struct accept_op : operation {
            accept_op(tcp_acceptor* acceptor) : acceptor_{acceptor}
            { }

            [[nodiscard]] constexpr bool await_ready() const noexcept
            {
                return false;
            }

            [[nodiscard]] tcp_socket await_resume() noexcept
            {
                return std::move(*socket_);
            }

            void await_suspend(std::coroutine_handle<> h) noexcept
            {
                waiting_coroutine_ = h;
                acceptor_->ev_.add_operation(&acceptor_->desc_, event_loop::op_type::READ_OP, this);
            }

            void perform() override
            {
                sockaddr_in client_addr{0};
                auto len = sizeof(client_addr);
                auto socket = ::accept(acceptor_->desc_.fd, reinterpret_cast<sockaddr *>(&client_addr),
                                       reinterpret_cast<socklen_t *>(&len));
                if (socket < 0) {
                    throw std::system_error(errno, std::generic_category());
                }
                socket_.emplace(socket);
                waiting_coroutine_.resume();
            }

            tcp_acceptor* acceptor_;
            std::optional<tcp_socket> socket_;
            std::coroutine_handle<> waiting_coroutine_;
        };

        event_loop &ev_;
        descriptor desc_;
    };
}
