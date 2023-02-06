#pragma once

#include "operation.h"

namespace aifs {
    struct handle : operation {
        handle() noexcept : id_{g_handle_id_++} {}
        virtual ~handle() = default;

        uint64_t id_;
    private:
        static uint64_t g_handle_id_;
    };

    struct handle_info {
        uint64_t id;
        handle* h;
    };
}