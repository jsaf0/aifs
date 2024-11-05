#pragma once

#include <system_error>

namespace aifs {
struct Operation {
    virtual void perform(const std::error_code&) { }
    std::error_code ec {};
};
} // namespace aifs
