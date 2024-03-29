#pragma once

#include <functional>
#include <map>
#include <string_view>

#include <spdlog/spdlog.h>

#include "aifs/http/request.h"
#include "aifs/http/router.h"
#include "aifs/task.h"


#include <optional>

namespace aifs::http {

using HandlerFn = std::function<Task<HandlerStatus>(const Request&, Response&)>;

namespace detail {
    struct Route {
        std::string path;
        HandlerFn fn;
    };

    struct Layer {
        std::optional<Route> route;
    };
}

class ExpressRouter : public Router {
public:
    Task<HandlerStatus> handle(const Request& req, Response& resp)
    {
        for (auto& layer : m_layers) {
            if (layer.route && (*layer.route).path == req.url()) {
                auto status = co_await (*layer.route).fn(req, resp);
                if (status == HandlerStatus::Accepted) {
                    co_return status;
                }
            }
        }
        // TODO: What if we didn't handle the request?
        co_return HandlerStatus::NotAccepted;
    }

    void get(const std::string& path, HandlerFn fn)
    {
        detail::Route route{path, std::move(fn)};
        detail::Layer layer{std::move(route)};
        m_layers.emplace_back(std::move(layer));
    }

private:
    std::vector<detail::Layer> m_layers;
};
}
