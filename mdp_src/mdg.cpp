// market data generator cpp file// 22.06.26// ZeroK

#include "mdg.hpp"
#include <immintrin.h>  // for rdtsc

Tick mdg::generate () {

    Tick tick {};

    tick.seq = ++_seq;
    tick.timestamp_tsc = __rdtsc();

    tick.bid = 100.05;
    tick.ask = 101.00;

    tick.bid_qty = 20;
    tick.bid_ask = 30;

    __builtin_memcpy (tick.symbol, "AAPL", 5);

    return tick;
}
