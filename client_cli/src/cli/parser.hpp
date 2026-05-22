#pragma once

#include <string>
#include <vector>

namespace cli {

struct Command {
    std::string              name;
    std::vector<std::string> args;
    bool                     empty() const { return name.empty(); }
};

Command parse(const std::string& line);

}