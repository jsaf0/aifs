#pragma once

#include "aifs/http/request.h"
#include "aifs/http/response.h"
#include "aifs/task.h"

namespace aifs::http {
/**
 * Generic router interface.
 */

enum class HandlerStatus {
    Accepted,
    NotAccepted
};

class Router {
public:
    virtual ~Router() = default;
    virtual Task<HandlerStatus> handle(const Request&, Response&) = 0;
};
}
