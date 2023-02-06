#pragma once

#include <string>

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

        void close() {
            ::close(fd_);
        }

        int fd_;
    };
}  // namespace aifs
