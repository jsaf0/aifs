#pragma once

#include <string>
#include <span>

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "non_copyable.h"
#include "awaitable.h"

namespace aifs {
    class tcp_socket : non_copyable {
    private:
        struct async_receive_op;
        struct async_send_op;

    public:
        tcp_socket(event_loop& ev, int fd, struct sockaddr_in addr) : ev_{ev}, desc_{fd}, addr_{addr}
        {
            ::fcntl(desc_.fd, F_SETFL, O_NONBLOCK);
        }

        [[nodiscard]] std::string remote_address() const
        {
            constexpr int max_len = 16;
            char buf[max_len];
            if (const char* p = inet_ntop(AF_INET, &addr_.sin_addr, buf, max_len)) {
                return {p, ::strlen(p)};
            }
            return "";
        }

        [[nodiscard]] unsigned short remote_port() const
        {
            return ntohs(addr_.sin_port);
        }

        [[nodiscard]] awaitable<ssize_t> receive(std::span<char> buffer)
        {
            return awaitable<int>{async_receive_op{*this, buffer}};
        }

        [[nodiscard]] awaitable<ssize_t> send(std::span<const char> buffer)
        {
            return awaitable<int>{async_send_op{*this, buffer}};
        }

        void close() const {
            ::close(desc_.fd);
        }

    private:
        struct async_receive_op : operation {
            async_receive_op(tcp_socket& socket, std::span<char> buffer) : socket_{socket}, buffer_{buffer} {}

            ssize_t await_resume()
            {
                if (auto ec = std::get_if<std::error_code>(&result_)) {
                    throw std::system_error(*ec);
                }
                return std::get<ssize_t>(result_);
            }

            void await_suspend(std::coroutine_handle<> h)
            {
                waiter_ = h;
                socket_.ev_.add_operation(&socket_.desc_, event_loop::op_type::read_op, this);
            }

            void perform(const std::error_code& ec) override
            {
                if (ec) {
                    result_ = ec;
                    waiter_.resume();
                    return;
                }

                ssize_t n = ::read(socket_.desc_.fd, buffer_.data(), buffer_.size());
                spdlog::warn("read = {}", n);
                if (n > 0) {
                    result_ = n;
                } else if (n == 0) {
                    result_ = std::make_error_code(std::errc::connection_aborted);
                } else {
                    result_ = std::make_error_code(static_cast<std::errc>(errno));
                }
                waiter_.resume();
            }

            tcp_socket& socket_;
            std::span<char> buffer_;
            std::variant<std::monostate, std::error_code, ssize_t> result_;
            std::coroutine_handle<> waiter_;
        };

        struct async_send_op : operation {
            async_send_op(tcp_socket& socket, std::span<const char> buffer) : socket_{socket}, buffer_{buffer} {}

            ssize_t await_resume()
            {
                if (auto ec = std::get_if<std::error_code>(&result_)) {
                    throw std::system_error(*ec);
                }
                return std::get<ssize_t>(result_);
            }

            void await_suspend(std::coroutine_handle<> h)
            {
                waiter_ = h;
                socket_.ev_.add_operation(&socket_.desc_, event_loop::op_type::write_op, this);
            }

            void perform(const std::error_code& ec) override
            {
                if (ec) {
                    result_ = ec;
                    waiter_.resume();
                    return;
                }

                ssize_t n = ::write(socket_.desc_.fd, buffer_.data(), buffer_.size());
                if (n > 0) {
                    result_ = n;
                } else {
                    result_ = std::make_error_code(static_cast<std::errc>(errno));
                }
                waiter_.resume();
            }

            tcp_socket& socket_;
            std::span<const char> buffer_;
            std::variant<std::monostate, std::error_code, ssize_t> result_;
            std::coroutine_handle<> waiter_;
        };

        event_loop& ev_;
        descriptor desc_;
        struct sockaddr_in addr_;
    };
}  // namespace aifs
