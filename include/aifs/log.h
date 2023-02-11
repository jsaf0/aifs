#pragma once

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>

namespace aifs {
using Logger = std::shared_ptr<spdlog::logger>;
Logger makeLogger(const std::string& name)
{
    return spdlog::stdout_color_mt(name);
}
} // namespace aifs
