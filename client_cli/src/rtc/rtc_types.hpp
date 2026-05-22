#pragma once

#include <functional>
#include <string>

namespace rtc_client {

using WsSendCallback = std::function<void(const std::string&)>;

}