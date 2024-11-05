#pragma once

#include <string>
#include <map>

#include "aifs/task.h"
#include "aifs/tcp_socket.h"

namespace aifs::http {
class Response {
public:
    Response(StreamSocket& socket);
    void setStatus(unsigned statusCode);
    void setHeader(const std::string& field, const std::string& value);
    Task<> send(std::string body);

private:
    StreamSocket& m_socket;
    bool m_sent;
    unsigned m_status;
    std::map<std::string, std::string> m_headers;
};
}
