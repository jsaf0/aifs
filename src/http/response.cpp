#include "aifs/http/response.h"

namespace aifs::http {
Response::Response(TCPSocket& socket)
    : m_socket{socket}
{}

void Response::setStatus(unsigned statusCode)
{
    m_status = statusCode;
}

void Response::setHeader(const std::string& field, const std::string& value)
{
    m_headers.emplace(field, value);
}

Task<> Response::send(std::string body)
{
    std::stringstream buf;
    buf << "HTTP/1.1 " << m_status << " OK\r\n";
    for (const auto& [f, v] : m_headers) {
        buf << f << ": " << v << "\r\n";
    }
    buf << "Content-Length: " << body.size() << "\r\n";
    buf << "\r\n";
    buf << body;
    co_await m_socket.send(buf.str());
    m_socket.close();
}
}
