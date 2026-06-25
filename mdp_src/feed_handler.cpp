// feed handler v1// 25.06.26// ZeroK

/* workflow:
 * MDG -> Ring -> Publisher -> NIC TX -> Feed handler RX -> observe ticks/stats
 * */

#include <immintrin.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_eth.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <cstdint>
#include <atomic>

#include "tick.hpp"
#include "thread_pinning.hpp"
#include "feed_handler.hpp"
#include "dpdk_port.hpp"
#include "global_shutdown.hpp"



namespace counters {

    std::atomic<std::uint64_t> gaps            {};
    std::atomic<std::uint64_t> received        {};
    std::atomic<std::uint64_t> latency_cycles  {};
}


void feed_handler::run_fh (dpdk::Port& port) {

        std::uint64_t expected  { 1 };
        rte_mbuf* pkts [cfg::BURST_SIZE];

        while (g_RUNNING) {

            std::uint16_t recd = 
                    rte_eth_rx_burst (
                        port.port_id,
                        0,
                        pkts,
                        cfg::BURST_SIZE
                    );
            if (recd == 0) { _mm_pause(); continue; }

            counters::received.fetch_add (recd, std::memory_order_relaxed);
            for (auto i {0u}; i < recd; ++i) {
                
                auto* pkt = pkts[i];
                if (pkt->pkt_len < cfg::PKT_SIZE) {
                    rte_pktmbuf_free (pkt);
                    continue;
                }

                auto* eth = rte_pktmbuf_mtod (pkt, rte_ether_hdr*);
                auto* ipv = reinterpret_cast <rte_ipv4_hdr*> (eth + 1);
                auto* udp = reinterpret_cast <rte_udp_hdr*>  (ipv + 1);
                const auto* tck = reinterpret_cast <const Tick*> (udp + 1); 
                
                // seq check, latency measure, stats
                counters::latency_cycles.store (__rdtsc() - tck->timestamp_tsc,
                                                std::memory_order_relaxed);

                if (tck->seq != expected) { 
                    counters::gaps.fetch_add (1, std::memory_order_relaxed);
                    expected = tck->seq + 1;
                }
                else ++expected;

                rte_pktmbuf_free (pkt);
            }   // for

        }   // while
    


        // while (g_RUNNING) {
        //
        //     std::this_thread::sleep_for (std::chrono::seconds(1));
        //
        //     std::uint64_t prev_received  {};
        //     auto recv_now  =  counters::received.load (std::memory_order_relaxed);
        //     auto recv_rate =  recv_now - prev_received; 
        //
        //     prev_received  =  recv_now;
        //
        //     std::printf ("received/s : %" PRIu64 "\n"
        //                  "gaps       : %" PRIu64 "\n"
        //                  "latency    : %" PRIu64 "\n\n",
        //                 recv_rate,
        //                 counters::gaps.load (std::memory_order_relaxed),
        //                 counters::latency_cycles.load (std::memory_order_relaxed)
        //             );
        // } // while
}


