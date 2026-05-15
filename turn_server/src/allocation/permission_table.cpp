#include "permission_table.hpp"

#include <boost/asio.hpp>

namespace allocation {

bool isDeniedAddress(const std::string& address) {
    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address_v4(address, ec);
    if (ec) return false;

    uint32_t ip = addr.to_uint();
    for (auto& [lo, hi] : kDeniedRanges) {
        if (ip >= lo && ip <= hi) return true;
    }
    return false;
}

}