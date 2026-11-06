#include "ReportUtil.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

std::string ReportUtil::generateReport(const Scheduler* scheduler) {
    std::ostringstream oss;

    int totalCores = scheduler->getTotalCores();

    // Take a SINGLE snapshot of every core's current process.
    // This guarantees that coresUsed and the running-process list
    // come from the same point in time — no more Core: -1 races.
    auto coreProcesses = scheduler->getPerCoreProcesses();

    int coresUsed = 0;
    for (const auto& p : coreProcesses) {
        if (p) coresUsed++;
    }
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

    oss << "--------------------------------------" << std::endl;
    oss << "Running processes:" << std::endl;
    // Use the core index as the authoritative core ID, not p->getCoreId(),
    // which can be stale (-1) during RR preemption.
    for (int i = 0; i < static_cast<int>(coreProcesses.size()); i++) {
        if (coreProcesses[i]) {
            const auto& p = coreProcesses[i];
            oss << p->getName() << "   "
                << "(" << p->getCreationTimestamp() << ")   "
                << "Core: " << i << "   "
                << p->getCurrentLine() << " / " << p->getTotalInstructions()
                << std::endl;
        }
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

    oss << "--------------------------------------" << std::endl;

    return oss.str();
}

void ReportUtil::writeToFile(const Scheduler* scheduler) {
    std::string report = generateReport(scheduler);

    std::ofstream file("csopesy-log.txt", std::ios::app);
    if (file.is_open()) {
        file << report;
        file.close();
        std::cout << "Report generated at csopesy-log.txt!" << std::endl;
    } else {
        std::cerr << "Error: Could not write to csopesy-log.txt" << std::endl;
    }
}
