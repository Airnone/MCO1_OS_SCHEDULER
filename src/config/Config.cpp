#include "Config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

static std::string stripQuotes(const std::string& s) {
    std::string result = s;
    result.erase(std::remove(result.begin(), result.end(), '"'), result.end());
    return result;
}

Config loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filename);
    }

    Config cfg;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key >> value)) continue;

        if (key == "num-cpu") {
            int v = std::stoi(value);
            if (v < 1 || v > 128)
                throw std::runtime_error("num-cpu out of range [1,128]: " + value);
            cfg.numCpu = v;
        }
        else if (key == "scheduler") {
            cfg.scheduler = stripQuotes(value);
            if (cfg.scheduler != "fcfs" && cfg.scheduler != "rr")
                throw std::runtime_error("scheduler must be 'fcfs' or 'rr': " + cfg.scheduler);
        }
        else if (key == "quantum-cycles") {
            unsigned long long v = std::stoull(value);
            if (v < 1 || v > UINT32_MAX)
                throw std::runtime_error("quantum-cycles out of range [1, 2^32]: " + value);
            cfg.quantumCycles = static_cast<uint32_t>(v);
        }
        else if (key == "batch-process-freq") {
            unsigned long long v = std::stoull(value);
            if (v < 1 || v > UINT32_MAX)
                throw std::runtime_error("batch-process-freq out of range [1, 2^32]: " + value);
            cfg.batchProcessFreq = static_cast<uint32_t>(v);
        }
        else if (key == "min-ins") {
            unsigned long long v = std::stoull(value);
            if (v < 1 || v > UINT32_MAX)
                throw std::runtime_error("min-ins out of range [1, 2^32]: " + value);
            cfg.minIns = static_cast<uint32_t>(v);
        }
        else if (key == "max-ins") {
            unsigned long long v = std::stoull(value);
            if (v < 1 || v > UINT32_MAX)
                throw std::runtime_error("max-ins out of range [1, 2^32]: " + value);
            cfg.maxIns = static_cast<uint32_t>(v);
        }
        else if (key == "delays-per-exec" || key == "delay-per-exec") {
            unsigned long long v = std::stoull(value);
            if (v > UINT32_MAX)
                throw std::runtime_error("delays-per-exec out of range [0, 2^32]: " + value);
            cfg.delaysPerExec = static_cast<uint32_t>(v);
        }
    }

    return cfg;
}
