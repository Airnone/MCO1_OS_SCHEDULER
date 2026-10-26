#include "Scheduler.h"
#include "../process/InstructionGenerator.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

Scheduler::Scheduler(const Config& config) : config_(config) {}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    running_ = true;

    // Create cores (no threads — they are tick-driven)
    for (int i = 0; i < config_.numCpu; i++) {
        cores_.push_back(std::make_unique<Core>(i));
    }

    // Start the single scheduler loop thread
    schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
}

void Scheduler::stop() {
    running_ = false;
    batchRunning_ = false;

    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }

    cores_.clear();
}

void Scheduler::enqueue(std::shared_ptr<Process> process) {
    process->setState(ProcessState::Ready);
    std::lock_guard<std::mutex> lock(queueMutex_);
    readyQueue_.push_back(process);
}

void Scheduler::startBatchGeneration() {
    if (batchRunning_) return;
    lastBatchSpawnTick_ = clock_.getTicks();
    batchRunning_ = true;
}

void Scheduler::stopBatchGeneration() {
    batchRunning_ = false;
}

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name) {
    auto instructions = InstructionGenerator::generate(
        name, config_.minIns, config_.maxIns);

    int id;
    {
        std::lock_guard<std::mutex> lock(processListMutex_);
        id = nextProcessId_++;
    }

    auto process = std::make_shared<Process>(id, name, std::move(instructions));

    {
        std::lock_guard<std::mutex> lock(processListMutex_);
        allProcesses_.push_back(process);
    }

    enqueue(process);
    return process;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) const {
    std::lock_guard<std::mutex> lock(processListMutex_);
    for (const auto& p : allProcesses_) {
        if (p->getName() == name) {
            return p;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<Process>> Scheduler::getRunningProcesses() const {
    std::vector<std::shared_ptr<Process>> result;
    std::lock_guard<std::mutex> lock(processListMutex_);
    for (const auto& p : allProcesses_) {
        ProcessState state = p->getState();
        if (state != ProcessState::Finished) {
            result.push_back(p);
        }
    }
    return result;
}

std::vector<std::shared_ptr<Process>> Scheduler::getFinishedProcesses() const {
    std::vector<std::shared_ptr<Process>> result;
    std::lock_guard<std::mutex> lock(processListMutex_);
    for (const auto& p : allProcesses_) {
        if (p->getState() == ProcessState::Finished) {
            result.push_back(p);
        }
    }
    return result;
}

int Scheduler::getCoresUsed() const {
    int count = 0;
    for (const auto& core : cores_) {
        if (core->isBusy()) {
            count++;
        }
    }
    return count;
}

// --- The central scheduler loop (runs on its own thread) ---

void Scheduler::schedulerLoop() {
    bool isRR = (config_.scheduler == "rr");
    uint32_t quantum = config_.quantumCycles;
    uint32_t delaysPerExec = config_.delaysPerExec;

    while (running_) {
        // 1. Wake up sleeping processes whose sleep time has expired
        {
            uint64_t currentTick = clock_.getTicks();
            auto it = sleepingProcesses_.begin();
            while (it != sleepingProcesses_.end()) {
                if (currentTick >= it->wakeUpTick) {
                    enqueue(it->process);
                    it = sleepingProcesses_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        // 2. Batch process generation
        if (batchRunning_) {
            uint64_t currentTick = clock_.getTicks();
            if (currentTick - lastBatchSpawnTick_ >= config_.batchProcessFreq) {
                batchCounter_++;
                std::ostringstream nameStream;
                if (batchCounter_ < 10) {
                    nameStream << "p0" << batchCounter_;
                } else {
                    nameStream << "p" << batchCounter_;
                }
                createProcess(nameStream.str());
                lastBatchSpawnTick_ = currentTick;
            }
        }

        // 3. Assign ready processes to free cores
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            for (auto& core : cores_) {
                if (!core->isBusy() && !readyQueue_.empty()) {
                    auto process = readyQueue_.front();
                    readyQueue_.pop_front();
                    core->assignProcess(process);
                }
            }
        }

        // 4. Execute one tick on each busy core
        for (auto& core : cores_) {
            if (!core->isBusy()) continue;

            // Check RR quantum BEFORE executing
            if (isRR && core->getTicksOnProcess() >= quantum) {
                auto process = core->removeProcess();
                if (process && !process->isFinished()) {
                    enqueue(process);  // Preempt back to ready queue
                }
                continue;
            }

            uint32_t dummy = 0;
            bool stillRunning = core->executeTick(delaysPerExec, dummy);

            if (!stillRunning) {
                auto process = core->getCurrentProcess();
                if (process) {
                    ProcessState state = process->getState();
                    if (state == ProcessState::Sleeping) {
                        auto p = core->removeProcess();
                        uint64_t wakeAt = clock_.getTicks() + p->getSleepTicks();
                        sleepingProcesses_.push_back({p, wakeAt});
                    } else if (state == ProcessState::Finished) {
                        core->removeProcess();
                        // Process stays in allProcesses_ for reporting
                    }
                }
            }
        }

        // 5. Advance the global CPU tick
        clock_.tick();

        // Small sleep to prevent 100% CPU usage (not wall-clock time — just prevents busy spin)
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}
