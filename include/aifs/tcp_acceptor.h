#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <fmt/core.h>
#include <sys/socket.h>

#include <coroutine>
#include <system_error>

#include "awaitable.h"
#include "descriptor.h"
#include "event_loop.h"
#include "operation.h"
#include "tcp_socket.h"

namespace aifs {
template <typename Socket>
class Acceptor {
public:
    using SocketPtr = std::unique_ptr<Socket>;

public:
    virtual ~Acceptor() = default;
    virtual void cancel() = 0;
    virtual Awaitable<SocketPtr> accept() = 0;
};

class TCPAcceptor : public Acceptor<TCPSocket> {
private:
    struct AcceptOp;

public:
    TCPAcceptor(EventLoop& ctx, int port)
        : m_eventLoop { ctx }
        , m_desc { -1 }
    {
        m_desc.m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_desc.m_fd < 0) {
            throw std::system_error(errno, std::generic_category(), "socket");
        }

        int ret = ::fcntl(m_desc.m_fd, F_SETFL, O_NONBLOCK);
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(), "fcntl");
        }

        int val = 1;
        ret = ::setsockopt(m_desc.m_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(),
                "setsockopt");
        }

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        ret = ::bind(m_desc.m_fd, (sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(), "bind");
        }

        ret = ::listen(m_desc.m_fd, SOMAXCONN);
        if (ret < 0) {
            throw std::system_error(errno, std::generic_category(), "listen");
        }
    }

    ~TCPAcceptor() override
    {
        if (m_desc.m_fd != -1) {
            ::close(m_desc.m_fd);
        }
    }

    void cancel() override
    {
        m_eventLoop.cancelOperation(&m_desc, EventLoop::OpType::read_op);
    }

    [[nodiscard]] Awaitable<SocketPtr> accept() override
    {
        if (m_desc.m_fd == -1) {
            throw std::runtime_error("Not ready to accept");
        }
        return Awaitable<SocketPtr> { AcceptOp { this } };
    }

private:
    struct AcceptOp : Operation {
        explicit AcceptOp(TCPAcceptor* acceptor)
            : m_acceptor { acceptor }
        {
        }

        SocketPtr await_resume()
        {
            if (auto ec = std::get_if<std::error_code>(&m_result)) {
                throw std::system_error(*ec);
            }
            return std::move(std::get<SocketPtr>(m_result));
        }

        void await_suspend(std::coroutine_handle<> h)
        {
            m_waiter = h;
            m_acceptor->m_eventLoop.addOperation(&m_acceptor->m_desc,
                EventLoop::OpType::read_op, this);
        }

        void perform(const std::error_code& ec) override
        {
            if (ec) {
                m_result = ec;
                m_waiter.resume();
                return;
            }

            struct sockaddr_in client_addr {
                0
            };
            auto len = sizeof(client_addr);
            auto fd = ::accept(m_acceptor->m_desc.m_fd,
                reinterpret_cast<sockaddr*>(&client_addr),
                reinterpret_cast<socklen_t*>(&len));
            if (fd < 0) {
                m_result = std::make_error_code(static_cast<std::errc>(errno));
            } else {
                m_result.emplace<SocketPtr>(std::make_unique<TCPSocket>(
                    m_acceptor->m_eventLoop, fd, client_addr));
            }
            m_waiter.resume();
        }

        TCPAcceptor* m_acceptor;
        std::variant<std::monostate, std::error_code, SocketPtr> m_result;
        std::coroutine_handle<> m_waiter;
    };

    EventLoop& m_eventLoop;
    Descriptor m_desc;
};
} // namespace aifs
