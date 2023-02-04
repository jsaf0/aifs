#pragma once

namespace aifs {
    struct handle {
        handle() noexcept : id_{g_handle_id_++} {}
        virtual ~handle() = default;
        virtual void run() = 0;

        uint64_t id_;
    private:
        static uint64_t g_handle_id_;
    };

    struct handle_info {
        uint64_t id;
        handle* h;
    };
}