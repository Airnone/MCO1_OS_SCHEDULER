#include "Core.h"

Core::Core(int id) : id_(id) {}

void Core::assignProcess(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(processMutex_);
    currentProcess_ = process;
    ticksOnProcess_ = 0;
    delayCyclesRemaining_ = 0;
    process->setState(ProcessState::Running);
    process->setCoreId(id_);
}

std::shared_ptr<Process> Core::removeProcess() {
    std::lock_guard<std::mutex> lock(processMutex_);
    auto p = currentProcess_;
    currentProcess_ = nullptr;
    if (p) {
        p->setCoreId(-1);
    }
    return p;
}

bool Core::executeTick(uint32_t delaysPerExec, uint32_t& /*delayCyclesLeft*/) {
    std::lock_guard<std::mutex> lock(processMutex_);
    if (!currentProcess_) return false;

    ticksOnProcess_++;

    // Handle busy-wait delay before executing an instruction
    if (delayCyclesRemaining_ > 0) {
        delayCyclesRemaining_--;
        return true;  // Process still on this core, just busy-waiting
    }

    // Execute one instruction
    bool canContinue = currentProcess_->executeNext(id_);

    if (canContinue) {
        // Set up delay for the next instruction
        delayCyclesRemaining_ = delaysPerExec;
        return true;
    }

    // Process either finished or went to sleep
    return false;
}

bool Core::isBusy() const {
    std::lock_guard<std::mutex> lock(processMutex_);
    return currentProcess_ != nullptr;
}

std::shared_ptr<Process> Core::getCurrentProcess() const {
    std::lock_guard<std::mutex> lock(processMutex_);
    return currentProcess_;
}
