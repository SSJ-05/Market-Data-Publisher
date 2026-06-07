// market data degenator mdp v1// 07.06.26// ZeroK

/* Market data generator
 * need random walk to simulate tick prices
 * use mt19937
 * */

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <random>   // for std::mt19937
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

// tick struct
struct alignas(64) Tick {

    std::uint64_t seq;              // client needs sequence number to detect packet loss
    std::uint64_t timestamp_ns;     // time when tick generated

    double bid;
    double ask;

    std::uint32_t bid_qty;
    std::uint32_t ask_qty;

    char symbol[16];
    char pad[4];

};


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
    
    auto now = std::chrono::steady_clock::now();
    tick.timestamp_ns = std::chrono::duration_cast<
                            std::chrono::nanoseconds>(
                                now.time_since_epoch()).count();

    std::memcpy (tick.symbol, "AAPL", 5);

    return tick;
}



int main () {
    // std::printf("size of tick : %d\n", sizeof(Tick));

    MDG gen;

    while (true) {
        Tick tick = gen.generate();

        std::printf ("seq=%llu  symbol=%s  bid=%.2f  ask=%.2f  bid_qty=%lu  ask_qty=%lu\n",
                tick.seq,
                tick.symbol,
                tick.bid,
                tick.ask,
                tick.bid_qty, 
                tick.ask_qty
                );

        constexpr auto next_tick { 1000 };
        std::this_thread::sleep_for (std::chrono::milliseconds(next_tick));
    }

    return EXIT_SUCCESS;
}
