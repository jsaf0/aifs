#pragma once

namespace aifs {
    struct descriptor {
        int fd_;
        operation* ops_[3 /* max op */];
    };
}
