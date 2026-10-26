#pragma once
#include <atomic>
#include <cstdint>

class Clock {
public:
    Clock() : ticks_(0) {}

    uint64_t getTicks() const { return ticks_.load(); }
    void tick() { ticks_++; }

private:
    std::atomic<uint64_t> ticks_;
};
