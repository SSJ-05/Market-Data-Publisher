// market data publisher// 22.06.26// ZeroK
// business logic here


/* this file contains
 * ring.pop()
 * burst assembly
 * mbuf alloc
 * tick to packet conversion
 * tx_burst
 * stats
 * */

#include "publisher.hpp"
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <immintrin.h>
#include <atomic>

namespace cfg {

    constexpr std::uint16_t BURST_SIZE { 1 << 5 };     // 32 batch size

    static_assert (BURST_SIZE <= 64, "BURST_SIZE TOO LARGE\n");
}



// counters for stats
alignas(64) std::atomic<std::size_t> sent_pkts {};
char pad_0 [64 - sizeof(std::atomic<std::size_t>)];

alignas(64) std::atomic<std::size_t> dropped_pkts {};
char pad_1 [64 - sizeof(std::atomic<std::size_t>)];



// helper func - burst builder
[[ nodiscard ]]
static std::uint16_t build_burst (  rte_mempool* pool,
                                    Tick ticks[],          
                                    rte_mbuf* pkts[],       
                                    std::uint16_t count) {

    std::uint16_t built {};
    for (; built < count; ++built) {
    
        Packet packet = 
            pkt_builder::build_pkt (ticks[built]);

        auto* pkt = rte_pktmbuf_alloc (pool);
        if (!pkt) break;

        void* dst = rte_pktmbuf_append (pkt, sizeof(Packet));
        if (!dst) {
            rte_pktmbuf_free (pkt);
            break;
        }

        __builtin_memcpy (dst, &packet, sizeof(Packet));

        pkts[built] = pkt;
    }

    return built;
}



struct burst_context {

    Tick ticks[cfg::BURST_SIZE];        // 32 ticks = 32 * 64
    rte_mbuf* pkts[cfg::BURST_SIZE];    // 32 mbufs - 32 * 8 (both in L1)
};


void publisher::run (dpdk::Port& port,
                     zerok::z_ring<Tick, (1 << 16)>& ring) {

    burst_context bc;
    while (RUNNING || !ring.empty()) {
        
        std::uint16_t count {};
        while (count < cfg::BURST_SIZE) {
            if (!ring.pop (bc.ticks[count]))
                break;

            ++count;
        }

        auto built = build_burst(
                        port.tx_pool,
                        bc.ticks,
                        bc.pkts,
                        count);

        if (built == 0) {
            _mm_pause();
            continue;
        }

        auto sent =
            rte_eth_tx_burst(
                    port.port_id,
                    0,
                    bc.pkts,
                    built);

        for (auto i {sent}; i < built; ++i) {
            rte_pktmbuf_free (bc.pkts[i]);
        }

        // increment counters
        sent_pkts.fetch_add (sent, std::memory_order_relaxed);
        dropped_pkts.fetch_add (built - sent, std::memory_order_relaxed);
    }
}
