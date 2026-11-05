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
        if (state == ProcessState::Running) {
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

ReportSnapshot Scheduler::snapshot() const {
    // Holding loopMutex_ means no tick is in progress, so core assignments,
    // process states and progress counters are all read at the same instant.
    std::lock_guard<std::mutex> loopLock(loopMutex_);
    ReportSnapshot snap;
    snap.totalCores = config_.numCpu;
    snap.coresUsed = 0;
    for (const auto& core : cores_) {
        auto p = core->getCurrentProcess();
        if (p) {
            snap.coresUsed++;
            snap.running.push_back({p, core->getId()});
        }
    }
    std::lock_guard<std::mutex> listLock(processListMutex_);
    for (const auto& p : allProcesses_) {
        if (p->getState() == ProcessState::Finished) {
            snap.finished.push_back(p);
        }
    }
    return snap;
}

std::string Scheduler::nextBatchName() {
    // p01, p02, ... ; skip names already taken (e.g. by "screen -s p05")
    while (true) {
        batchCounter_++;
        std::ostringstream nameStream;
        nameStream << "p" << std::setw(2) << std::setfill('0') << batchCounter_;
        if (!findProcess(nameStream.str())) return nameStream.str();
    }
}

// --- The central scheduler loop (runs on its own thread) ---
//
// One pass = one CPU tick. Order within a tick:
//   1. wake sleepers   2. batch generation   3. dispatch onto free cores
//   4. execute one tick per busy core   5. RR preemption + dispatch again
// Step 5 refills a core freed by a quantum expiry, SLEEP or finished process before
// the tick ends, so no core sits idle for a tick while the ready queue has work.

void Scheduler::schedulerLoop() {
    const bool isRR = (config_.scheduler == "rr");
    const uint32_t quantum = config_.quantumCycles;
    const uint32_t delaysPerExec = config_.delaysPerExec;

    while (running_) {
        {
            std::lock_guard<std::mutex> loopLock(loopMutex_);
            const uint64_t currentTick = clock_.getTicks();

            // 1. Wake up sleeping processes whose sleep time has expired
            for (auto it = sleepingProcesses_.begin(); it != sleepingProcesses_.end();) {
                if (currentTick >= it->wakeUpTick) {
                    auto p = it->process;
                    if (p->hasRemaining()) {
                        enqueue(p);
                    } else {
                        p->markFinished();  // SLEEP was its last instruction
                    }
                    it = sleepingProcesses_.erase(it);
                } else {
                    ++it;
                }
            }

            // 2. Batch process generation: one new process every batch-process-freq ticks
            if (batchRunning_ && currentTick - lastBatchSpawnTick_ >= config_.batchProcessFreq) {
                createProcess(nextBatchName());
                lastBatchSpawnTick_ = currentTick;
            }

            auto preemptExpired = [&]() {
                if (!isRR) return;
                for (auto& core : cores_) {
                    if (core->isBusy() && core->getTicksOnProcess() >= quantum) {
                        auto process = core->removeProcess();
                        if (process && !process->isFinished()) {
                            enqueue(process);  // back of the ready queue
                        }
                    }
                }
            };
            auto dispatch = [&]() {
                std::lock_guard<std::mutex> lock(queueMutex_);
                for (auto& core : cores_) {
                    if (!core->isBusy() && !readyQueue_.empty()) {
                        auto process = readyQueue_.front();
                        readyQueue_.pop_front();
                        core->assignProcess(process);
                    }
                }
            };

            // 3. Fill any free core (e.g. with processes that just woke or were created)
            dispatch();

            // 4. Execute one tick on each busy core
            for (auto& core : cores_) {
                if (!core->isBusy()) continue;

                uint32_t dummy = 0;
                bool stillRunning = core->executeTick(delaysPerExec, dummy);
                if (stillRunning) continue;

                auto process = core->getCurrentProcess();
                if (!process) continue;
                ProcessState state = process->getState();
                if (state == ProcessState::Sleeping) {
                    auto p = core->removeProcess();
                    // SLEEP(X): skip the next X ticks, run again on tick +X+1
                    uint64_t wakeAt = currentTick + 1 + p->getSleepTicks();
                    sleepingProcesses_.push_back({p, wakeAt});
                } else if (state == ProcessState::Finished) {
                    core->removeProcess();  // stays in allProcesses_ for reporting
                }
            }

            // 5. End of tick: Round Robin preemption, then refill freed cores right away,
            //    so between ticks (when reports are taken) no core idles while work waits.
            preemptExpired();
            dispatch();

            // 6. Advance the global CPU tick
            clock_.tick();
        }

        // Yield briefly so the CLI thread can take snapshots (not a wall-clock tick)
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
}
