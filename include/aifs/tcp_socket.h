#pragma once

#include <string>

#include <spdlog/spdlog.h>
#include <unistd.h>

#include "non_copyable.h"

namespace aifs {
    class tcp_socket : non_copyable {
    public:
        tcp_socket(int fd) : fd_{fd} {}

        [[nodiscard]] std::string remote_address() const {
            return "?";
        }

        [[nodiscard]] unsigned short remote_port() const {
            return 0;
        }

        void send(const std::string& resp) {
            // TODO: Implement asynchronous send/receive!
            char buf[1024];
            ::read(fd_, buf, 1024);
            ::write(fd_, resp.data(), resp.size());
        }

        void close() {
            ::close(fd_);
        }

        int fd_;
    };
}  // namespace aifs
