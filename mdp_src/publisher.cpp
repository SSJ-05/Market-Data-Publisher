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
#include <eth_dev.h>
#include <immintrin.h>

namespace cfg {

    constexpr std::uint16_t BURST_SIZE { 1 << 5 };     // 32 batch size
}


// helper func - burst builder
static std::uint16_t build_burst ( rte_mempool* pool,
                                    Tick ticks[],
                                    rte_mbuf* pkts[],
                                    std::uint16_t count) {

    std::uint16_t built {};

    for (; built < count; ++built) {
    
        Packet packet = build_pkt (ticks[built]);

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



void publisher::run (dpdk::Port& port,
                     zerok::z_ring<Tick, (1 << 16)>& ring) {

    Tick ticks[cfg::BURST_SIZE];

    rte_mbuf* pkts[cfg::BURST_SIZE];

    while (RUNNING) {

        std::uint16_t count {};

        while (count < cfg::BURST_SIZE) {

            if (!ring.pop(ticks[count]))
                break;

            ++count;
        }

        if (count == 0) {
            _mm_pause();
            continue;
        }

        build_burst(
                port.tx_pool,
                ticks,
                pkts,
                count);

        std::uint16_t sent =
            rte_eth_tx_burst(
                    port.port_id,
                    0,
                    pkts,
                    count);

        for (std::uint16_t i {sent}; i < count; ++i) {
            rte_pktmbuf_free(pkts[i]);
        }
    }
}
