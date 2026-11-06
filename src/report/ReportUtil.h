#pragma once
#include <string>
#include "../scheduler/Scheduler.h"

class ReportUtil {
public:
    // Generate the report string (used by both screen -ls and report-util)
    static std::string generateReport(const Scheduler* scheduler);

    // Write report to csopesy-log.txt
    static void writeToFile(const Scheduler* scheduler);
};
