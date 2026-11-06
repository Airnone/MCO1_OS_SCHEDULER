#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>
#include "Instruction.h"

enum class ProcessState { Ready, Running, Sleeping, Finished };

struct LogEntry {
    int coreId;
    std::string timestamp;  // MM/DD/YYYY hh:mm:ssAM/PM
    std::string message;
};

class Process {
public:
    Process(int id, const std::string& name, std::vector<Instruction> instructions);

    int getId() const;
    std::string getName() const;

    ProcessState getState() const;
    void setState(ProcessState state);

    int getCoreId() const;
    void setCoreId(int coreId);

    uint32_t getTotalInstructions() const;
    uint32_t getCurrentLine() const;
    bool isFinished() const;

    // Execute one instruction. Returns true if still runnable this tick.
    // Returns false if finished or if a SLEEP was hit (caller checks state).
    bool executeNext(int coreId);

    // Variable table
    uint16_t getVariable(const std::string& name);
    void setVariable(const std::string& name, uint16_t val);

    // Logs
    void addLog(int coreId, const std::string& message);
    std::vector<LogEntry> getLogs() const;

    // Creation timestamp
    std::string getCreationTimestamp() const;

    // SLEEP support
    uint8_t getSleepTicks() const;
    void decrementSleep();

private:
    int id_;
    std::string name_;
    ProcessState state_ = ProcessState::Ready;
    int coreId_ = -1;

    std::vector<Instruction> instructions_;

    // Execution stack for nested FOR loops
    struct ExecFrame {
        const std::vector<Instruction>* instrList;
        size_t pc;
        uint32_t iteration;
        uint32_t maxIterations;
    };
    std::vector<ExecFrame> execStack_;
    uint32_t totalInstructions_;
    uint32_t executedCount_ = 0;

    std::map<std::string, uint16_t> variables_;
    std::vector<LogEntry> logs_;
    mutable std::mutex mutex_;
    std::string creationTimestamp_;

    uint8_t sleepTicksRemaining_ = 0;

    static uint32_t countInstructions(const std::vector<Instruction>& instrs);
    static std::string getCurrentTimestamp();
    uint16_t resolveOperand(const Operand& op);
    void clampedSet(const std::string& name, int32_t val);
};
