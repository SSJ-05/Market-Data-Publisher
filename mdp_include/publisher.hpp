// market data publisher header file// 22.06.26// ZeroK

#pragma once

#include "dpdk_port.hpp"
#include "ring_buffer_v3.hpp"
#include "tick.hpp"
#include <cstddef>


namespace cfg {

    constexpr std::size_t  RING_SIZE  { 1 << 16 }; // 65536
}


namespace publisher {

    void run (dpdk::Port& port, 
            zerok::z_ring<Tick, cfg::RING_SIZE>& ring); 
} 
