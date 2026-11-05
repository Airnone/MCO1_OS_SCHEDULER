#include "ReportUtil.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

std::string ReportUtil::generateReport(const Scheduler* scheduler) {
    // One consistent snapshot, so "Cores used" always equals the number of
    // running rows and every running row has a real core id.
    ReportSnapshot snap = scheduler->snapshot();
    std::ostringstream oss;

    int coresAvailable = snap.totalCores - snap.coresUsed;
    int utilization = snap.totalCores > 0 ? (snap.coresUsed * 100) / snap.totalCores : 0;

    oss << "CPU utilization: " << utilization << "%" << std::endl;
    oss << "Cores used: " << snap.coresUsed << std::endl;
    oss << "Cores available: " << coresAvailable << std::endl;
    oss << std::endl;

    // Column widths: names padded to the longest name, status column fixed
    size_t nameWidth = 9;
    for (const auto& r : snap.running) nameWidth = std::max(nameWidth, r.process->getName().size());
    for (const auto& p : snap.finished) nameWidth = std::max(nameWidth, p->getName().size());
    nameWidth += 3;
    const int statusWidth = 13;

    auto row = [&](const std::shared_ptr<Process>& p, const std::string& status, uint32_t done) {
        oss << std::left << std::setw(static_cast<int>(nameWidth)) << p->getName()
            << "(" << p->getCreationTimestamp() << ")    "
            << std::setw(statusWidth) << status
            << done << " / " << p->getTotalInstructions() << std::endl;
    };

    oss << "--------------------------------------" << std::endl;
    oss << "Running processes:" << std::endl;
    for (const auto& r : snap.running) {
        row(r.process, "Core: " + std::to_string(r.coreId), r.process->getCurrentLine());
    }

    oss << std::endl;
    oss << "Finished processes:" << std::endl;
    for (const auto& p : snap.finished) {
        row(p, "Finished", p->getTotalInstructions());
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
        std::cout << "Report generated at C:/csopesy-log.txt!" << std::endl;
    } else {
        std::cerr << "Error: Could not write to csopesy-log.txt" << std::endl;
    }
}
