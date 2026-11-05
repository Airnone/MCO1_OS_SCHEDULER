#include "Process.h"
#include <chrono>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <iomanip>

Process::Process(int id, const std::string& name, std::vector<Instruction> instructions)
    : id_(id), name_(name), instructions_(std::move(instructions)) {
    totalInstructions_ = countInstructions(instructions_);
    creationTimestamp_ = getCurrentTimestamp();

    // Initialize the execution stack with the top-level instruction list
    ExecFrame frame;
    frame.instrList = &instructions_;
    frame.pc = 0;
    frame.iteration = 0;
    frame.maxIterations = 1;
    execStack_.push_back(frame);
}

int Process::getId() const { return id_; }
std::string Process::getName() const { return name_; }

ProcessState Process::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void Process::setState(ProcessState state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

int Process::getCoreId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return coreId_;
}

void Process::setCoreId(int coreId) {
    std::lock_guard<std::mutex> lock(mutex_);
    coreId_ = coreId;
}

uint32_t Process::getTotalInstructions() const {
    return totalInstructions_;
}

uint32_t Process::getCurrentLine() const {
    return executedCount_.load();
}

bool Process::isFinished() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == ProcessState::Finished;
}

std::string Process::getCreationTimestamp() const {
    return creationTimestamp_;
}

std::vector<LogEntry> Process::getLogs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logs_;
}

void Process::addLog(int coreId, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    LogEntry entry;
    entry.coreId = coreId;
    entry.timestamp = getCurrentTimestamp();
    entry.message = message;
    logs_.push_back(entry);
}

uint8_t Process::getSleepTicks() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sleepTicksRemaining_;
}

void Process::decrementSleep() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sleepTicksRemaining_ > 0) {
        sleepTicksRemaining_--;
    }
}

// --- Execution ---

uint16_t Process::resolveOperand(const Operand& op) {
    if (std::holds_alternative<std::string>(op)) {
        return getVariable(std::get<std::string>(op));
    }
    return std::get<uint16_t>(op);
}

uint16_t Process::getVariable(const std::string& name) {
    auto it = variables_.find(name);
    if (it == variables_.end()) {
        variables_[name] = 0;
        return 0;
    }
    return it->second;
}

void Process::setVariable(const std::string& name, uint16_t val) {
    variables_[name] = val;
}

void Process::clampedSet(const std::string& name, int32_t val) {
    if (val < 0) val = 0;
    if (val > 65535) val = 65535;
    variables_[name] = static_cast<uint16_t>(val);
}

// Advance the execution stack to the next real (non-FOR) instruction:
// finished FOR bodies repeat or pop, and FOR headers push a new frame.
// Leaves the stack empty when the program is done.
void Process::normalizeStack() {
    while (!execStack_.empty()) {
        auto& frame = execStack_.back();

        if (frame.pc >= frame.instrList->size()) {
            frame.iteration++;
            if (frame.iteration < frame.maxIterations) {
                frame.pc = 0;  // repeat the FOR body
            } else {
                execStack_.pop_back();  // block finished
            }
            continue;
        }

        const Instruction& instr = (*frame.instrList)[frame.pc];
        if (instr.type != InstructionType::For) return;

        frame.pc++;
        if (instr.repeatCount == 0) continue;
        ExecFrame forFrame;
        forFrame.instrList = &instr.body;
        forFrame.pc = 0;
        forFrame.iteration = 0;
        forFrame.maxIterations = instr.repeatCount;
        execStack_.push_back(forFrame);  // FOR itself is not counted as a line
    }
}

bool Process::hasRemaining() {
    normalizeStack();
    return !execStack_.empty();
}

void Process::markFinished() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = ProcessState::Finished;
}

bool Process::executeNext(int coreId) {
    // Called only from the scheduler thread.
    normalizeStack();
    if (execStack_.empty()) {
        markFinished();
        return false;
    }

    auto& frame = execStack_.back();
    const Instruction& instr = (*frame.instrList)[frame.pc];
    frame.pc++;

    bool slept = false;
    switch (instr.type) {
        case InstructionType::Print: {
            std::string output = instr.msg;
            if (!instr.varName.empty()) {
                output += std::to_string(getVariable(instr.varName));
            }
            addLog(coreId, output);
            break;
        }
        case InstructionType::Declare:
            variables_[instr.varName] = instr.value;
            break;
        case InstructionType::Add: {
            int32_t a = static_cast<int32_t>(resolveOperand(instr.src1));
            int32_t b = static_cast<int32_t>(resolveOperand(instr.src2));
            clampedSet(instr.dest, a + b);
            break;
        }
        case InstructionType::Subtract: {
            int32_t a = static_cast<int32_t>(resolveOperand(instr.src1));
            int32_t b = static_cast<int32_t>(resolveOperand(instr.src2));
            clampedSet(instr.dest, a - b);
            break;
        }
        case InstructionType::Sleep: {
            std::lock_guard<std::mutex> lock(mutex_);
            sleepTicksRemaining_ = instr.sleepTicks;
            state_ = ProcessState::Sleeping;
            slept = true;
            break;
        }
        case InstructionType::For:
            break;  // unreachable: normalizeStack() expands FOR headers
    }
    executedCount_++;

    // Finish in the same tick as the last instruction, so the core is
    // released immediately instead of idling for an extra tick.
    if (!hasRemaining()) {
        if (!slept) {
            markFinished();
            return false;
        }
        // A trailing SLEEP still sleeps; the scheduler finishes the process on wake-up.
    }
    return !slept;
}

// --- Static helpers ---

uint32_t Process::countInstructions(const std::vector<Instruction>& instrs) {
    uint32_t count = 0;
    for (const auto& instr : instrs) {
        if (instr.type == InstructionType::For) {
            count += instr.repeatCount * countInstructions(instr.body);
        } else {
            count++;
        }
    }
    return count;
}

std::string Process::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif

    // Format: MM/DD/YYYY hh:mm:ssAM/PM
    std::ostringstream oss;
    int hour12 = tm_buf.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampm = (tm_buf.tm_hour >= 12) ? "PM" : "AM";

    oss << std::setfill('0')
        << std::setw(2) << (tm_buf.tm_mon + 1) << "/"
        << std::setw(2) << tm_buf.tm_mday << "/"
        << (tm_buf.tm_year + 1900) << " "
        << std::setw(2) << hour12 << ":"
        << std::setw(2) << tm_buf.tm_min << ":"
        << std::setw(2) << tm_buf.tm_sec
        << ampm;

    return oss.str();
}
