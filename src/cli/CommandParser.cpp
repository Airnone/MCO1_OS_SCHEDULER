#include "CommandParser.h"
#include <sstream>

ParsedCommand CommandParser::parse(const std::string& input) {
    ParsedCommand result;
    std::istringstream iss(input);
    std::string token;

    if (iss >> result.command) {
        while (iss >> token) {
            result.args.push_back(token);
        }
    }

    return result;
}
