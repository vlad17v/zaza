#pragma once

#include <string>
#include <stdexcept>

namespace net {

struct HttpResponse {
    unsigned    status;
    std::string body;
};

struct HttpError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class HttpClient {
public:
    HttpClient(const std::string& host,
               uint16_t           port,
               bool               verify_cert = false);

    HttpResponse post(const std::string& path,
                      const std::string& body,
                      const std::string& content_type = "application/json");

    HttpResponse get(const std::string& path,
                     const std::string& bearer_token = "");

private:
    std::string host_;
    uint16_t    port_;
    bool        verify_cert_;
};

}