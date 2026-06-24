// market data publisher header file// 22.06.26// ZeroK

#pragma once

#include "dpdk_port.hpp"
#include "ring_buffer_v3.hpp"
#include "tick.hpp"
#include <cstddef>
#include <cstdio>
#include <atomic>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>


// counters for stats
namespace counters {
    extern std::atomic<std::size_t> sent_pkts;
    extern std::atomic<std::size_t> dropped_pkts;

    void get_publisher_stats () noexcept; 

}   // namespace counters


namespace cfg {
 
    constexpr std::uint16_t PKT_SIZE {  sizeof(rte_ether_hdr) +
                                        sizeof(rte_ipv4_hdr) +
                                        sizeof(rte_udp_hdr) +
                                        sizeof(Tick) };

    constexpr std::uint16_t BURST_SIZE { 1 << 5 };      // 32 batch size

    constexpr std::size_t  RING_SIZE  { 1 << 16 };      // 65536

    static_assert (BURST_SIZE <= 1 << 6, "BURST_SIZE TOO LARGE\n");
}


namespace publisher {

    void run (dpdk::Port& port, 
            zerok::z_ring<Tick, cfg::RING_SIZE>& ring); 
} 
