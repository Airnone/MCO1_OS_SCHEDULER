#include "ReportUtil.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

std::string ReportUtil::generateReport(const Scheduler* scheduler) {
    std::ostringstream oss;

    int totalCores = scheduler->getTotalCores();
    int coresUsed = scheduler->getCoresUsed();
    int coresAvailable = totalCores - coresUsed;

    // CPU utilization
    double utilization = 0.0;
    if (totalCores > 0) {
        utilization = (static_cast<double>(coresUsed) / totalCores) * 100.0;
    }

    oss << "CPU utilization: " << static_cast<int>(utilization) << "%" << std::endl;
    oss << "Cores used: " << coresUsed << std::endl;
    oss << "Cores available: " << coresAvailable << std::endl;
    oss << std::endl;

    // Running processes
    oss << "Running processes:" << std::endl;
    auto running = scheduler->getRunningProcesses();
    for (const auto& p : running) {
        oss << p->getName() << "   "
            << "(" << p->getCreationTimestamp() << ")   "
            << "Core: " << p->getCoreId() << "   "
            << p->getCurrentLine() << " / " << p->getTotalInstructions()
            << std::endl;
    }

    oss << std::endl;

    // Finished processes
    oss << "Finished processes:" << std::endl;
    auto finished = scheduler->getFinishedProcesses();
    for (const auto& p : finished) {
        oss << p->getName() << "   "
            << "(" << p->getCreationTimestamp() << ")   "
            << "Finished   "
            << p->getTotalInstructions() << " / " << p->getTotalInstructions()
            << std::endl;
    }

    return oss.str();
}

void ReportUtil::writeToFile(const Scheduler* scheduler) {
    std::string report = generateReport(scheduler);

    std::ofstream file("csopesy-log.txt", std::ios::trunc);
    if (file.is_open()) {
        file << report;
        file.close();
        std::cout << "Report generated at C:/csopesy-log.txt!" << std::endl;
    } else {
        std::cerr << "Error: Could not write to csopesy-log.txt" << std::endl;
    }
}
