#include "ScreenManager.h"
#include "../report/ReportUtil.h"
#include <iostream>
#include <sstream>

ScreenManager::ScreenManager(Scheduler* scheduler)
    : scheduler_(scheduler) {}

bool ScreenManager::createScreen(const std::string& name) {
    // Check if process already exists
    auto existing = scheduler_->findProcess(name);
    if (existing) {
        std::cout << "Process \"" << name << "\" already exists." << std::endl;
        return false;
    }

    // Create the process (enqueues it automatically)
    scheduler_->createProcess(name);

    // Clear console and attach to the process screen
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    runProcessScreen(name);
    return true;
}

bool ScreenManager::reattachScreen(const std::string& name) {
    auto process = scheduler_->findProcess(name);
    if (!process || process->isFinished()) {
        std::cout << "Process " << name << " not found." << std::endl;
        return false;
    }

#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif

    runProcessScreen(name);
    return true;
}

void ScreenManager::listScreens() const {
    std::string report = ReportUtil::generateReport(scheduler_);
    std::cout << report;
}

void ScreenManager::runProcessScreen(const std::string& processName) {
    auto process = scheduler_->findProcess(processName);
    if (!process) return;

    std::string input;
    while (true) {
        std::cout << "Process: " << processName << std::endl;
        std::cout << "process-screen> ";
        std::getline(std::cin, input);

        if (input == "exit") {
            // Return to main menu
#ifdef _WIN32
            system("cls");
#else
            system("clear");
#endif
            return;
        }
        else if (input == "process-smi") {
            std::cout << renderProcessSmi(process) << std::endl;
        }
        else {
            std::cout << "Unrecognized command. Use 'process-smi' or 'exit'." << std::endl;
        }
    }
}

std::string ScreenManager::renderProcessSmi(const std::shared_ptr<Process>& process) {
    std::ostringstream oss;

    oss << "Process: " << process->getName() << std::endl;
    oss << "ID: " << process->getId() << std::endl;
    oss << std::endl;

    // Print logs
    auto logs = process->getLogs();
    if (!logs.empty()) {
        for (const auto& log : logs) {
            oss << "(" << log.timestamp << ") "
                << "Core:" << log.coreId << " "
                << "\"" << log.message << "\"" << std::endl;
        }
        oss << std::endl;
    }

    oss << "Current instruction line: " << process->getCurrentLine() << std::endl;
    oss << "Lines of code: " << process->getTotalInstructions() << std::endl;

    if (process->isFinished()) {
        oss << "Finished!" << std::endl;
    }

    return oss.str();
}
