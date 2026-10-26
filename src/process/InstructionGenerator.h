#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "Instruction.h"

class InstructionGenerator {
public:
    static std::vector<Instruction> generate(const std::string& processName,
                                              uint32_t minIns, uint32_t maxIns);

private:
    static std::vector<Instruction> generateBlock(const std::string& processName,
                                                   uint32_t count, int nestLevel,
                                                   int& varCounter);
    static uint32_t countFlat(const std::vector<Instruction>& instrs);
};
