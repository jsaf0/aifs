#pragma once

namespace aifs::http {
class Request {
public:
    Request(unsigned method, std::string url)
        : m_method{method}
        , m_url{std::move(url)}
    {}

    unsigned method() const {
        return m_method;
    }

    const std::string& url() const {
        return m_url;
    }

    unsigned m_method;
    std::string m_url;
};
}
