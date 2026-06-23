// market data producer consumer mdp v4// 22.06.26// ZeroK

/* workflow: 
      MDG thread -> ring buffer -> publisher thread -> tx_burst (DPDK) -> UDP client 
 * bottleneck :  linux network stack that caps throughput to 134k ticks/s
                 while "ring_buffer_v3" is capable of 40M ticks/s
                 packets loss : 0.4%
 * FIX: DPDK for kernel bypass
 */


#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>

#include <rte_cycles.h>

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cinttypes>    // for PRIu64/32
#include <cstring>

#include <chrono>
#include <thread>
#include <atomic>

#include <type_traits>
#include <immintrin.h>  // for _mm_pause

#include "ring_buffer_v3.hpp"
#include "thread_pinning.hpp"
#include "tick.hpp"


namespace cfg {
    
    constexpr std::uint16_t  MPOOL_SIZE     { 8192 };
    constexpr std::uint16_t  RX_DESC        { 1024 };
    constexpr std::uint16_t  TX_DESC        { 1024 };
    constexpr std::uint16_t  CACHE_SIZE     { 256 };
    constexpr std::uint16_t  BURST          { 32 };
    constexpr std::uint16_t  RX_Q           { 1 };
    constexpr std::uint16_t  TX_Q           { 1 };
    constexpr std::uint16_t  PORT           { 0 };

    static_assert (MPOOL_SIZE >= 2 * TX_DESC, "INCREASE THE MPOOL_SIZE\n");

}

inline std::uint64_t now_tsc () {

    // return __rdtsc();           // gcc intrinsic
    return rte_get_tsc_cycles();
}

struct alignas(64) Packet {
    
    rte_ether_hdr   eth;
    rte_ipv4_hdr    ip;
    rte_udp_hdr     udp;
    Tick            tick;
};

// for graceful shutdown on ctrl + c
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }



constexpr int MAX_SIZE { 1 << 16 };   // 65536
alignas(64) zerok::z_ring<Tick, MAX_SIZE> ring;

alignas(64) std::atomic<std::uint64_t> produced { 0 };
char pad0 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> prev_produced { 0 };
char pad1 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> sent { 0 };
char pad2 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> prev_sent { 0 };
char pad3 [64 - sizeof(std::atomic<std::uint64_t>)]; 


// market data generator MDG class 
class MDG {
private:
    std::uint64_t seq_;

public:
    MDG() : 
        seq_(0) {}

    Tick generate();
};


// helper func for latency measurement
// inline std::uint64_t now_ns () {
//     return std::chrono::duration_cast<
//             std::chrono::nanoseconds>(
//                 std::chrono::steady_clock::now()
//                 .time_since_epoch()).count();
// }


// generator logic
Tick MDG::generate() {

    Tick tick {};
    tick.seq = ++seq_;
    tick.timestamp_ns = now_tsc ();
    
    return tick;
}


// producer thread
void producer () {
    
    pin_thread (0);

    MDG gen;
    while (RUNNING) {
        Tick tick = gen.generate ();    // generate unconditionally

        while (!ring.push(tick)) {
            if (!RUNNING) return;
            _mm_pause();
        }
        
        produced.fetch_add (1, std::memory_order_relaxed);
    }
}


// publisher thread
// 1 ring pop -> 1 send becomes 
// 32 ring pop -> 1 tx_burst (DPDK batch processing)
void publisher () {
    
    pin_thread (1);

    Tick tick {};
    std::uint64_t expected { 1 };

    while (RUNNING || !ring.empty()) {       // drain remaining ticks after shutdown

        rte_mbuf* pkts [cfg::BURST];

        std::uint16_t count {};

        while (count < cfg::BURST && ring.pop(tick)) {
            
            while (!ring.pop(tick)) {
                if (!RUNNING && ring.empty()) return;
                _mm_pause();
            }

            pkts [count++] = tick; 
        }

        int ret = 
            rte_eth_tx_burst (
                cfg::PORT, 0 , pkts, count
            );
        if (ret < 0) rte_exit (EXIT_FAILURE, "TX BURST FAILED.\n");
    }
}



int main () {
    // check if Ticks can be blasted thru network
    static_assert (std::is_trivially_copyable_v<Tick>, "Tick must be blit-able");
    static_assert (sizeof(Tick) == 64, "Tick size must be 64 bytes\n");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

    /***********************************************************************************************************/

    std::thread prod_thread (producer);
    std::thread publ_thread (publisher);

    while (RUNNING) {
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        auto prod_now  =  produced.load (std::memory_order_relaxed);
        auto cons_now  =  sent.load (std::memory_order_relaxed);

        auto prod_rate =  prod_now - prev_produced.load (std::memory_order_relaxed);
        auto cons_rate =  cons_now - prev_sent.load (std::memory_order_relaxed);

        prev_produced.store (prod_now, std::memory_order_relaxed);
        prev_sent.store (cons_now, std::memory_order_relaxed);

        std::printf ("produced/s : %" PRIu64 "\n"
                     "sent/s     : %" PRIu64 "\n"
                     "depth      : %zu\n\n",
                    prod_rate,
                    cons_rate,
                    ring.size()
                );
        
        // std::printf ("latency    : %" PRIu64 "\n\n",
        //             latency_ns.load (std::memory_order_relaxed));
    }

    prod_thread.join();
    publ_thread.join();
    

    shutdown (socketfd, SHUT_RDWR);
    close (socketfd);

    std::printf("\n\n=== Publisher Shutdown ===\n\n");

    return EXIT_SUCCESS;
}
