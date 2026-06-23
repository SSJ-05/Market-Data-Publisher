// market data publisher header file// 22.06.26// ZeroK

#pragma once

#include <cstdint>
#include "dpdk_port.hpp"
#include "ring_buffer_v3.hpp"
#include "tick.hpp"

namespace publisher {

    void run (dpdk::Port& port, 
            zerok::z_ring<Tick, (1 << 16)>& ring); // 65536 bytes size ring

} // namespace publisher
