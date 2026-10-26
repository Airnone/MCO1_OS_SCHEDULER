#pragma once
#include <memory>
#include <atomic>
#include "../process/Process.h"

class Scheduler;

class Core {
public:
    Core(int id);

    int getId() const { return id_; }

    // Assign a process to this core for execution
    void assignProcess(std::shared_ptr<Process> process);

    // Remove the current process from this core
    std::shared_ptr<Process> removeProcess();

    // Execute one tick's worth of work on the current process
    // Returns true if the process is still running on this core
    bool executeTick(uint32_t delaysPerExec, uint32_t& delayCyclesLeft);

    bool isBusy() const;
    std::shared_ptr<Process> getCurrentProcess() const;

    // Track ticks spent on current process (for RR quantum)
    uint32_t getTicksOnProcess() const { return ticksOnProcess_; }
    void resetTicksOnProcess() { ticksOnProcess_ = 0; }

private:
    int id_;
    std::shared_ptr<Process> currentProcess_;
    mutable std::mutex processMutex_;
    uint32_t ticksOnProcess_ = 0;
    uint32_t delayCyclesRemaining_ = 0;
};
