#pragma once
#include <string>
#include <memory>
#include "../scheduler/Scheduler.h"

class ScreenManager {
public:
    ScreenManager(Scheduler* scheduler);

    // screen -s <name>: create and attach. Returns true if successful.
    bool createScreen(const std::string& name);

    // screen -r <name>: reattach. Returns true if successful.
    bool reattachScreen(const std::string& name);

    // screen -ls: print listing to stdout
    void listScreens() const;

    // Runs the per-process screen loop (process-smi / exit)
    void runProcessScreen(const std::string& processName);

    // Render process-smi output for a process
    static std::string renderProcessSmi(const std::shared_ptr<Process>& process);

private:
    Scheduler* scheduler_;
};
