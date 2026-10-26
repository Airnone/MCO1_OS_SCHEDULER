#pragma once
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <string>
#include <thread>
#include "../config/Config.h"
#include "../process/Process.h"
#include "Core.h"
#include "Clock.h"

class Scheduler {
public:
    Scheduler(const Config& config);
    ~Scheduler();

    void start();
    void stop();

    // Enqueue a process into the ready queue
    void enqueue(std::shared_ptr<Process> process);

    // scheduler-start / scheduler-stop
    void startBatchGeneration();
    void stopBatchGeneration();

    // Create a process by name (screen -s)
    std::shared_ptr<Process> createProcess(const std::string& name);

    // Lookup processes
    std::shared_ptr<Process> findProcess(const std::string& name) const;
    std::vector<std::shared_ptr<Process>> getRunningProcesses() const;
    std::vector<std::shared_ptr<Process>> getFinishedProcesses() const;

    // Config access
    const Config& getConfig() const { return config_; }
    Clock& getClock() { return clock_; }

    // Number of active (non-idle) cores
    int getCoresUsed() const;
    int getTotalCores() const { return config_.numCpu; }

    bool isRunning() const { return running_; }

private:
    Config config_;
    Clock clock_;

    std::vector<std::unique_ptr<Core>> cores_;

    // Ready queue
    std::deque<std::shared_ptr<Process>> readyQueue_;
    mutable std::mutex queueMutex_;

    // All processes ever created (for lookup)
    std::vector<std::shared_ptr<Process>> allProcesses_;
    mutable std::mutex processListMutex_;

    // Sleeping processes
    struct SleepEntry {
        std::shared_ptr<Process> process;
        uint64_t wakeUpTick;
    };
    std::vector<SleepEntry> sleepingProcesses_;

    // Batch generation
    std::atomic<bool> batchRunning_{false};
    int batchCounter_ = 0;
    uint64_t lastBatchSpawnTick_ = 0;

    int nextProcessId_ = 1;

    std::atomic<bool> running_{false};

    // The single scheduler thread that drives the tick loop
    std::thread schedulerThread_;
    void schedulerLoop();
};
