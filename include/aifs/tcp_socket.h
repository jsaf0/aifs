#pragma once

#include <span>
#include <string>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>

#include <spdlog/spdlog.h>

#include "awaitable.h"
#include "non_copyable.h"

namespace aifs {
class TCPSocket : NonCopyable {
private:
    struct ReceiveOp;
    struct SendOp;

public:
    TCPSocket(EventLoop& ev, int fd, struct sockaddr_in addr)
        : m_eventLoop { ev }
        , m_desc { fd }
        , m_addr { addr }
    {
        ::fcntl(m_desc.m_fd, F_SETFL, O_NONBLOCK);
    }

    [[nodiscard]] std::string remote_address() const
    {
        constexpr int max_len = 16;
        char buf[max_len];
        if (const char* p = inet_ntop(AF_INET, &m_addr.sin_addr, buf, max_len)) {
            return { p, ::strlen(p) };
        }
        return "";
    }

    [[nodiscard]] unsigned short remote_port() const { return ntohs(m_addr.sin_port); }

    [[nodiscard]] Awaitable<ssize_t> receive(std::span<char> buffer)
    {
        return Awaitable<int> { ReceiveOp { *this, buffer } };
    }

    [[nodiscard]] Awaitable<ssize_t> send(std::span<const char> buffer)
    {
        return Awaitable<int> { SendOp { *this, buffer } };
    }

    void close() const { ::close(m_desc.m_fd); }

private:
    struct ReceiveOp : Operation {
        ReceiveOp(TCPSocket& socket, std::span<char> buffer)
            : socket_ { socket }
            , buffer_ { buffer }
        {
        }

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
            socket_.m_eventLoop.addOperation(&socket_.m_desc, EventLoop::OpType::read_op, this);
        }

        void perform(const std::error_code& ec) override
        {
            if (ec) {
                result_ = ec;
                waiter_.resume();
                return;
            }

            ssize_t n = ::read(socket_.m_desc.m_fd, buffer_.data(), buffer_.size());
            if (n > 0) {
                result_ = n;
            } else if (n == 0) {
                result_ = std::make_error_code(std::errc::connection_aborted);
            } else {
                result_ = std::make_error_code(static_cast<std::errc>(errno));
            }
            waiter_.resume();
        }

        TCPSocket& socket_;
        std::span<char> buffer_;
        std::variant<std::monostate, std::error_code, ssize_t> result_;
        std::coroutine_handle<> waiter_;
    };

    struct SendOp : Operation {
        SendOp(TCPSocket& socket, std::span<const char> buffer)
            : m_socket { socket }
            , m_buffer { buffer }
        {
        }

        ssize_t await_resume()
        {
            if (auto ec = std::get_if<std::error_code>(&m_result)) {
                throw std::system_error(*ec);
            }
            return std::get<ssize_t>(m_result);
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            m_waiter = h;
            m_socket.m_eventLoop.addOperation(&m_socket.m_desc, EventLoop::OpType::write_op, this);
        }

        void perform(const std::error_code& ec) override
        {
            if (ec) {
                m_result = ec;
                m_waiter.resume();
                return;
            }

            ssize_t n = ::write(m_socket.m_desc.m_fd, m_buffer.data(), m_buffer.size());
            if (n > 0) {
                m_result = n;
            } else {
                m_result = std::make_error_code(static_cast<std::errc>(errno));
            }
            m_waiter.resume();
        }

        TCPSocket& m_socket;
        std::span<const char> m_buffer;
        std::variant<std::monostate, std::error_code, ssize_t> m_result;
        std::coroutine_handle<> m_waiter;
    };

    EventLoop& m_eventLoop;
    Descriptor m_desc;
    struct sockaddr_in m_addr;
};
} // namespace aifs
