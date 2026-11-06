#pragma once
#include <memory>
#include "../scheduler/Scheduler.h"
#include "../screen/ScreenManager.h"

class Console {
public:
    Console();
    ~Console();

    // Run the main menu loop (blocks until exit)
    void run();

private:
    void printBanner() const;
    void handleCommand(const std::string& input);

    bool initialized_ = false;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<ScreenManager> screenManager_;
};
