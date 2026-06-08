// market data producer consumer mdp v2// 08.06.26// ZeroK

/* workflow: producer thread -> ring buffer -> consumer thread -> stdout 
 *
 * */

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cinttypes>    // for PRIu64/32
#include <cstring>

#include <random>   // for std::mt19937
#include <chrono>

#include <thread>
#include <atomic>

#include <type_traits>
#include <immintrin.h>  // for _mm_pause
#include <signal.h>

#include "ring_buffer_v2.1.hpp"


// for graceful shutdown on ctrl + c
volatile sig_atomic_t RUNNING { 1 };
void sig_handler (int sig) { RUNNING = 0; }



// tick struct
struct alignas(64) Tick {

    std::uint64_t seq;              // client needs sequence number to detect packet loss
    std::uint64_t timestamp_ns;     // time when tick generated

    double bid;
    double ask;

    std::uint32_t bid_qty;
    std::uint32_t ask_qty;

    char symbol[8];
    char pad[16];

};



constexpr int MAX_SIZE { 0x400 };   // 1024
alignas(64) zerok::z_ring<Tick, MAX_SIZE> ring;

alignas(64) std::atomic<std::uint64_t> produced { 0 };
alignas(64) std::atomic<std::uint64_t> prev_produced { 0 };
char pad1 [64 - sizeof(std::atomic<std::uint64_t>)]; 

alignas(64) std::atomic<std::uint64_t> consumed { 0 };
alignas(64) std::atomic<std::uint64_t> prev_consumed { 0 };
char pad2 [64 - sizeof(std::atomic<std::uint64_t>)]; 


// market data generator MDG class 
class MDG {
private:
    std::uint64_t seq_;
    double mid_price_;

    std::mt19937 rng_;
    std::uniform_int_distribution<int> ud_;

public:
    MDG() : 
        seq_(0), 
        mid_price_(100.00),
        rng_(std::random_device {} ()),
        ud_(-2, 2) {}

    Tick generate();
};



// generator logic
Tick MDG::generate() {
    Tick tick {};
    ++seq_;
    
    mid_price_ += ud_(rng_) * 0.01;
    tick.seq = seq_;

    tick.bid = mid_price_ - 0.01;
    tick.ask = mid_price_ + 0.01;

    tick.bid_qty = 500;
    tick.ask_qty = 300;

    tick.timestamp_ns = std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                std::chrono::steady_clock::now()
                                .time_since_epoch()).count();

    std::memcpy (tick.symbol, "AAPL", 5);

    return tick;
}


// producer thread
void producer () {
    
    MDG gen;
    while (RUNNING) {
        Tick tick = gen.generate ();

        while (!ring.push(tick)) {
            _mm_pause();
        }
        
        produced.fetch_add (1, std::memory_order_relaxed);
    }
}


// consumer thread
alignas(64) std::atomic<std::uint64_t> latency_ns { 0 };
void consumer () {
    
    Tick tick;
    std::uint64_t expected { 1 };

    while (RUNNING) {
        while (!ring.pop(tick)) {
            _mm_pause();
            continue;
        }

        if (tick.seq != expected) {
            std::printf ("gap expected : %" PRIu64
                         "got : %" PRIu64 "\n", expected, tick.seq);

        }

        consumed.fetch_add (1, std::memory_order_relaxed);

        expected += tick.seq + 1;

        auto now_ns = std::chrono::duration_cast<
                        std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now()
                            .time_since_epoch()).count();
        
        latency_ns.store (now_ns - tick.timestamp_ns, std::memory_order_relaxed);
    }
}

int main () {
    // check if Ticks can be blasted thru network
    static_assert (std::is_trivially_copyable_v<Tick>, "Tick must be blit-able");

    signal (SIGINT,  sig_handler);
    signal (SIGTERM, sig_handler);

    std::printf("size of tick : %zu\n", sizeof(Tick));


    std::thread pt (producer);
    std::thread ct (consumer);

    auto prod_now = produced.load (std::memory_order_relaxed);
    auto cons_now = consumed.load (std::memory_order_relaxed);

    auto prod_rate = prod_now - prev_produced.load (std::memory_order_relaxed);
    auto cons_rate = cons_now - prev_consumed.load (std::memory_order_relaxed);

    prev_produced.store (prod_now, std::memory_order_release);
    prev_consumed.store (cons_now, std::memory_order_release);

    while (RUNNING) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
        std::printf ("produced/s : %" PRIu64 "\n"
                     "consumed/s : %" PRIu64 "\n"
                     "depth    : %zu\n",
                     // "latency  : %" PRIu64 "\n\n",
                    prev_produced.load (std::memory_order_relaxed),
                    prev_consumed.load (std::memory_order_relaxed),
                    ring.size()
                    // latency_ns
                );
    }

    pt.join();
    ct.join();

    return EXIT_SUCCESS;
}
