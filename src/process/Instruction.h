#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <variant>

enum class InstructionType { Print, Declare, Add, Subtract, Sleep, For };

// An operand is either a variable name (string) or a literal uint16_t
using Operand = std::variant<std::string, uint16_t>;

struct Instruction {
    InstructionType type;

    // PRINT
    std::string msg;
    std::string varName;  // optional variable to interpolate in PRINT; also used for DECLARE

    // DECLARE
    uint16_t value = 0;

    // ADD / SUBTRACT
    std::string dest;
    Operand src1;
    Operand src2;

    // SLEEP
    uint8_t sleepTicks = 0;

    // FOR
    std::vector<Instruction> body;
    uint32_t repeatCount = 0;
};
