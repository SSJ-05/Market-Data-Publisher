// market data generator/ tick generate header file// 22.06.26// ZeroK

#pragma once

#include <cstdint>
#include <atomic>
#include "tick.hpp"

extern std::atomic<std::size_t> produced_ticks;

class MDG {
private:
    std::uint64_t _seq {};

public:
    Tick generate ();
};

