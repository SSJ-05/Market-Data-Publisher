// feed handler v2.0// 26.06.26// ZeroK

/* integrated DPDK
 *
 * */


#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cinttypes>
#include <csignal>

#include <thread>
#include <atomic>
#include <chrono>

#include <immintrin.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "tick.hpp"
#include "thread_pinning.hpp"
#include "dpdk_port.hpp"
#include "global_shutdown.hpp"


volatile sig_atomic_t g_RUNNING { 1 };
void sig_handler (int sig) { g_RUNNING = 0; }


int main (int argc, char** argv) {

    std::printf("\n\n=== Feed Handler v2.0 ===\n\n");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

    auto port = dpdk::init (argc, argv);

    /**********************************************************************************************/

    std::atomic<std::uint64_t> gaps           {};
    std::atomic<std::uint64_t> received       {};
    std::atomic<std::uint64_t> latency_tsc    {};

    constexpr std::uint16_t BURST_SIZE { 1 << 5 };
    rte_mbuf* pkts [BURST_SIZE];

    std::thread receiver ([&] () {
        pin_thread (0);

        std::uint64_t expected  { 1 };

        while (g_RUNNING) {

            std::uint16_t recvd = 
                    rte_eth_rx_burst (
                        port.port_id,
                        0,
                        pkts,
                        BURST_SIZE
                    );
            if (recvd == 0) { _mm_pause(); continue; }

            received.fetch_add (recvd, std::memory_order_relaxed);


            for (auto i {0}; i < recvd; ++i) {

                    auto* pkt = pkts[i];

                    // pkt len validation
                    if (pkt->pkt_len < sizeof(rte_ether_hdr)
                                        + sizeof(rte_ipv4_hdr)
                                        + sizeof(rte_udp_hdr)
                                        + sizeof(Tick))
                    { rte_pktmbuf_free(pkt); continue; }
                    
                    // packet parsing
                    auto* eth = rte_pktmbuf_mtod (pkt, rte_ether_hdr*);
                    auto* ipv = reinterpret_cast<rte_ipv4_hdr*>(eth + 1);
                    auto* udp = reinterpret_cast<rte_udp_hdr*>(ipv + 1);
                    auto* tck = reinterpret_cast<Tick*>(udp + 1);

                    // sequence tracking
                    static bool once {};
                    if (!once) { 
                        std::printf ("First packet received.\n");
                        once = true;
                    }

                    if (tck->seq > expected) {
                        gaps.fetch_add (tck->seq - expected, std::memory_order_relaxed);
                        expected = tck->seq + 1;
                    }
                    else ++expected;
                    
                    rte_pktmbuf_free (pkt);
             }   // for (...)
                
        }   // while
    });

    /***********************************************************************************************/

    std::thread reporter ([&] () {
        pin_thread (1);
        std::uint64_t prev_received  { 0 };

        while (g_RUNNING) {

            std::this_thread::sleep_for (std::chrono::seconds(1));

            auto recv_now  =  received.load (std::memory_order_relaxed);
            auto recv_rate =  recv_now - prev_received; 

            prev_received  =  recv_now;

            std::printf ("\nreceived/s : %" PRIu64 "\n",
                         // "gaps       : %" PRIu64 "\n\n",
                         // "latency    : %" PRIu64 "\n\n",
                        recv_rate
                        // gaps.load (std::memory_order_relaxed)
                        // latency_tsc.load (std::memory_order_relaxed)
                    );
        } // while
     });


    // shutdown on ctrl + c
    receiver.join();
    reporter.join();

    dpdk::shutdown (port);

    std::printf("\n\n=== Client terminated ===\n");


    std::printf("\n\n");
    return EXIT_SUCCESS;
}
