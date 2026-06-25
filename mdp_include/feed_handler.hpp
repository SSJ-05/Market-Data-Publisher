// feed handler header file// 25.06.26// ZeroK

# pragma once

#include <cstdint>
#include <atomic>

#include "tick.hpp"
#include "dpdk_port.hpp"
#include "global_shutdown.hpp"


namespace counters {

    extern std::atomic<std::uint64_t> gaps;
    extern std::atomic<std::uint64_t> received;
    extern std::atomic<std::uint64_t> latency_cycles;
}


namespace cfg {

    constexpr std::uint16_t BURST_SIZE  { 1 << 5 };
}


namespace feed_handler {

    void run_fh (dpdk::Port& port);
}


