#pragma once

#include <string>
#include <vector>

namespace allocation {

static const std::vector<std::pair<uint32_t,uint32_t>> kDeniedRanges = {
    {0x0A000000, 0x0AFFFFFF}, // 10.0.0.0/8
    {0xAC100000, 0xAC1FFFFF}, // 172.16.0.0/12
    //{0xC0A80000, 0xC0A8FFFF}, // 192.168.0.0/16
    //{0x7F000000, 0x7FFFFFFF}, // 127.0.0.0/8
};

bool isDeniedAddress(const std::string& address);

}