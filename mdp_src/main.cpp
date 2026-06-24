// Execution here// 23.06.26// ZeroK

/* architecture
 * main
 │
 ├── DPDK init
 │
 ├── producer thread
 │     MDG
 │       ↓
 │     ring.push()
 │
 ├── publisher thread
 │     ring.pop()
 │       ↓
 │     build_burst()
 │       ↓
 │     tx_burst()
 │
 ├── stats loop
 │
 └── shutdown
 *
 * */


#include "global_shutdown.hpp"
#include "dpdk_port.hpp"
#include "mdg.hpp"
#include "publisher.hpp"
#include "ring_buffer_v3.hpp"
#include "thread_pinning.hpp"

#include <thread>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <immintrin.h>


// check pool availability
static void pool_stats (rte_mempool* pool) {

    std::printf ("\navailable mempool: %u\n"
                 "in use mempool   : %u\n\n",
                 rte_mempool_avail_count (pool),
                 rte_mempool_in_use_count (pool));
}


int main (int argc, char** argv) {
    
    signal (SIGINT,  sig_handler);   // interrupt on external signal
    signal (SIGTERM, sig_handler);   // terminate signal

    auto port = dpdk::init (argc, argv);

    constexpr std::size_t RING_SIZE { 1 << 16 };
    zerok::z_ring<Tick, RING_SIZE> ring;

    std::atomic<std::size_t> produced_ticks {};

    MDG gen;

    std::thread producer_thread ([&] () {
            pin_thread (0);

            while (g_RUNNING) {
                
                Tick tick = gen.generate ();

                while(!ring.push (tick)) {
                    if (!g_RUNNING) return;
                    _mm_pause();
                }

                // increment counter
                produced_ticks.fetch_add (1, std::memory_order_relaxed);
            }
        });


    std::thread publisher_thread ([&] () {
            pin_thread (1);

            publisher::run (port, ring);
        });


    // stats counters
    std::size_t prev_prod {};
    std::size_t prev_sent {};
    std::size_t prev_drop {};


    // stats loop
    while (g_RUNNING) {
        std::this_thread::sleep_for (std::chrono::seconds (1));

        auto prod = produced_ticks.load (std::memory_order_relaxed);
        auto sent = counters::sent_pkts.load (std::memory_order_relaxed);
        auto drop = counters::dropped_pkts.load (std::memory_order_relaxed);

        std::printf (   "produced/s : %zu\n"
                        "sent/s     : %zu\n"
                        "dropped/s  : %zu\n"
                        "depth      : %zu\n\n",
                        prod - prev_prod,
                        sent - prev_sent,
                        drop - prev_drop,
                        ring.size()
                    );

        prev_prod = prod;
        prev_sent = sent;
        prev_drop = drop;

        pool_stats (port.tx_pool);
    }


    // shutdown sequence
    g_RUNNING = 0;
    producer_thread.join();
    publisher_thread.join();

    dpdk::shutdown (port);


    return EXIT_SUCCESS;
}
