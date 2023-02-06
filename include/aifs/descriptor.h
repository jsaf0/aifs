#pragma once

#include "event_loop.h"

namespace aifs {
    struct descriptor {


        int fd_;
        std::queue<operation*> ops_[4];
    };
}
