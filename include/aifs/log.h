#pragma once

#include <memory>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace aifs {
    using logger = std::shared_ptr<spdlog::logger>;
    logger make_logger(const std::string& name)
    {
        return spdlog::stdout_color_mt(name);
    }
}
