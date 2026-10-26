#include "InstructionGenerator.h"
#include <random>

static thread_local std::mt19937 rng(std::random_device{}());

static int randInt(int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

std::vector<Instruction> InstructionGenerator::generate(
    const std::string& processName, uint32_t minIns, uint32_t maxIns) {

    uint32_t target;
    if (minIns >= maxIns) {
        target = minIns;
    } else {
        std::uniform_int_distribution<uint32_t> dist(minIns, maxIns);
        target = dist(rng);
    }

    int varCounter = 0;
    return generateBlock(processName, target, 0, varCounter);
}

std::vector<Instruction> InstructionGenerator::generateBlock(
    const std::string& processName, uint32_t count, int nestLevel, int& varCounter) {

    std::vector<Instruction> result;
    uint32_t remaining = count;

    while (remaining > 0) {
        // Decide whether to generate a FOR loop
        bool canFor = (nestLevel < 3) && (remaining > 4);
        bool doFor = canFor && (randInt(1, 5) == 1);  // ~20% chance

        if (doFor) {
            Instruction instr;
            instr.type = InstructionType::For;
            instr.repeatCount = static_cast<uint32_t>(randInt(2, 5));

            uint32_t bodyTarget = remaining / instr.repeatCount;
            if (bodyTarget < 1) bodyTarget = 1;

            instr.body = generateBlock(processName, bodyTarget, nestLevel + 1, varCounter);
            uint32_t flatCost = instr.repeatCount * countFlat(instr.body);
            if (flatCost > remaining) flatCost = remaining;
            remaining -= flatCost;

            result.push_back(std::move(instr));
        } else {
            // Pick a simple instruction type (0..4)
            int choice = randInt(0, 4);
            Instruction instr;

            switch (choice) {
                case 0: {  // PRINT
                    instr.type = InstructionType::Print;
                    instr.msg = "Hello world from " + processName + "!";
                    break;
                }
                case 1: {  // DECLARE
                    instr.type = InstructionType::Declare;
                    instr.varName = "var" + std::to_string(varCounter++);
                    instr.value = static_cast<uint16_t>(randInt(0, 65535));
                    break;
                }
                case 2: {  // ADD
                    instr.type = InstructionType::Add;
                    instr.dest = "var" + std::to_string(randInt(0, std::max(0, varCounter - 1)));
                    // src1 and src2: 50/50 variable or literal
                    if (varCounter > 0 && randInt(0, 1) == 0) {
                        instr.src1 = std::string("var" + std::to_string(randInt(0, varCounter - 1)));
                    } else {
                        instr.src1 = static_cast<uint16_t>(randInt(0, 100));
                    }
                    if (varCounter > 0 && randInt(0, 1) == 0) {
                        instr.src2 = std::string("var" + std::to_string(randInt(0, varCounter - 1)));
                    } else {
                        instr.src2 = static_cast<uint16_t>(randInt(0, 100));
                    }
                    break;
                }
                case 3: {  // SUBTRACT
                    instr.type = InstructionType::Subtract;
                    instr.dest = "var" + std::to_string(randInt(0, std::max(0, varCounter - 1)));
                    if (varCounter > 0 && randInt(0, 1) == 0) {
                        instr.src1 = std::string("var" + std::to_string(randInt(0, varCounter - 1)));
                    } else {
                        instr.src1 = static_cast<uint16_t>(randInt(0, 100));
                    }
                    if (varCounter > 0 && randInt(0, 1) == 0) {
                        instr.src2 = std::string("var" + std::to_string(randInt(0, varCounter - 1)));
                    } else {
                        instr.src2 = static_cast<uint16_t>(randInt(0, 100));
                    }
                    break;
                }
                case 4: {  // SLEEP
                    instr.type = InstructionType::Sleep;
                    instr.sleepTicks = static_cast<uint8_t>(randInt(1, 5));
                    break;
                }
            }

            result.push_back(std::move(instr));
            remaining--;
        }
    }

    return result;
}

uint32_t InstructionGenerator::countFlat(const std::vector<Instruction>& instrs) {
    uint32_t count = 0;
    for (const auto& instr : instrs) {
        if (instr.type == InstructionType::For) {
            count += instr.repeatCount * countFlat(instr.body);
        } else {
            count++;
        }
    }
    return count;
}
