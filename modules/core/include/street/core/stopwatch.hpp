#pragma once

#include <chrono>

namespace street {

class Stopwatch {
public:
    using clock = std::chrono::steady_clock;

    Stopwatch() noexcept : start_(clock::now()) {}
    void restart() noexcept { start_ = clock::now(); }
    double seconds() const noexcept {
        return std::chrono::duration<double>(clock::now() - start_).count();
    }
    double millis() const noexcept { return seconds() * 1e3; }

private:
    clock::time_point start_;
};

} // namespace street
