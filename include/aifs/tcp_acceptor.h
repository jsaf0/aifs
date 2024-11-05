#pragma once

#include <coroutine>

#include <fmt/core.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "event_loop.h"
#include "operation.h"
#include "descriptor.h"

namespace aifs {
    class tcp_acceptor {
    private:
        struct accept_op;

    public:
        tcp_acceptor(io_context &ctx, int port) : ctx_{ctx}, port_{port}, d_{-1} {
            d_.fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (d_.fd_ < 0) {
                throw std::system_error(errno, std::generic_category(), "socket");
            }

            int val = 1;
            int ret = ::setsockopt(d_.fd_, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "setsockopt");
            }

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port_);
            ret = ::bind(d_.fd_, (sockaddr*)&addr, sizeof(addr));
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "bind");
            }

            ret = ::listen(d_.fd_, SOMAXCONN);
            if (ret < 0) {
                throw std::system_error(errno, std::generic_category(), "listen");
            }

            ctx.add_descriptor(&d_);
        }

        ~tcp_acceptor() {
            // TODO: Remove descriptor
            if (d_.fd_ != -1) {
                ::close(d_.fd_);
            }
        }

        [[nodiscard]] accept_op async_accept() {
            if (d_.fd_ == -1) {
                throw std::runtime_error("Not ready to accept");
            }
            return {this};
        }

    private:
        struct accept_op : operation {
            accept_op(tcp_acceptor* acceptor) : acceptor_{acceptor} {
                fmt::print("accept_op ctor\n");
            }

            ~accept_op() {
                fmt::print("accept_op dtor\n");
            }

            [[nodiscard]] constexpr bool await_ready() const noexcept {
                return false;
            }

            [[nodiscard]] tcp_socket await_resume() noexcept {
                fmt::print("accept_op await_resume\n");
                return std::move(*socket_);
            }

            void await_suspend(std::coroutine_handle<> h) noexcept {
                fmt::print("accept_op await_suspend\n");
                waiting_coroutine_ = h;
                acceptor_->ctx_.add_op(&acceptor_->d_, event_loop::op_type::READ_OP, this);
            }

            void perform() override {
                fmt::print("accept_op perform\n");

                sockaddr_in client_addr{0};
                auto len = sizeof(client_addr);
                auto socket = ::accept(acceptor_->d_.fd_, reinterpret_cast<sockaddr *>(&client_addr), reinterpret_cast<socklen_t *>(&len));
                if (socket < 0) {
                    throw std::system_error(errno, std::generic_category());
                }
                fmt::print("got conn = {}:{}\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

                socket_.emplace(socket);
                waiting_coroutine_.resume();
            }

#if 0
            void run() override {
                fmt::print("accept_op ready\n");

                sockaddr_in client_addr{0};
                auto len = sizeof(client_addr);
                auto socket = ::accept(acceptor_->d_.fd_, reinterpret_cast<sockaddr *>(&client_addr), reinterpret_cast<socklen_t *>(&len));
                if (socket < 0) {
                    throw std::system_error(errno, std::generic_category());
                }

                fmt::print("got conn = {}:{}\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                waiting_coroutine_.resume();
            }
#endif
            tcp_acceptor* acceptor_;
            std::optional<tcp_socket> socket_;
            std::coroutine_handle<> waiting_coroutine_;
        };

        io_context &ctx_;
        int port_;
        descriptor d_;
    };
}
