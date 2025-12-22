#pragma once
#include <chrono>

namespace utils {

class Timer {
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_point;
public:
    Timer() : start_point(Clock::now()) {}
    
    void reset() { start_point = Clock::now(); }
    
    // 返回毫秒
    double elapsed_ms() {
        auto end_point = Clock::now();
        return std::chrono::duration<double, std::milli>(end_point - start_point).count();
    }
};

} // namespace utils