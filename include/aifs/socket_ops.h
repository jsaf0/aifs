#pragma once

#include <sys/socket.h>

namespace aifs {
    namespace socket_ops {
        int create_socket(int domain, int type, int protocol) {
            return ::socket(domain, type, protocol);
        }

        void do_close(int fd) {
            ::close(fd);
        }
    }
}
