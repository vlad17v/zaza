#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace transport {

struct Endpoint {
    std::string address;
    uint16_t    port;

    bool operator==(const Endpoint& o) const {
        return address == o.address && port == o.port;
    }
};

using ReceiveCallback = std::function<void(const uint8_t*   data,
                                           size_t            size,
                                           const Endpoint&   from)>;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual void run()                                   = 0;
    virtual void stop()                                  = 0;
    virtual void send(const std::vector<uint8_t>& data,
                      const Endpoint&             to)    = 0;
    virtual void onReceive(ReceiveCallback callback)     = 0;
};

}