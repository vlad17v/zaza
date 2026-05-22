#include "parser.hpp"

#include <sstream>

namespace cli {

Command parse(const std::string& line) {
    Command cmd;
    std::istringstream iss(line);
    std::string token;

    if (!(iss >> token)) return cmd;
    cmd.name = token;

    while (iss >> token)
        cmd.args.push_back(token);

    return cmd;
}

}