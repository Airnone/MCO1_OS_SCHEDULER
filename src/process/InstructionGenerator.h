#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "Instruction.h"

class InstructionGenerator {
public:
    // Switch between normal random generation and the deterministic
    // Case-B pattern (alternating PRINT / ADD on variable "x").
    enum class Mode { Random, CaseB };
    static Mode mode;

    static std::vector<Instruction> generate(const std::string& processName,
                                              uint32_t minIns, uint32_t maxIns);

private:
    static std::vector<Instruction> generateBlock(const std::string& processName,
                                                   uint32_t count, int nestLevel,
                                                   int& varCounter);
    static std::vector<Instruction> generateCaseB(const std::string& processName,
                                                   uint32_t minIns, uint32_t maxIns);
    static uint32_t countFlat(const std::vector<Instruction>& instrs);
};
