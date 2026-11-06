#pragma once
#include <string>
#include <vector>

struct ParsedCommand {
    std::string command;                // e.g. "screen", "initialize", "exit"
    std::vector<std::string> args;      // e.g. ["-s", "myprocess"]
};

class CommandParser {
public:
    static ParsedCommand parse(const std::string& input);
};
