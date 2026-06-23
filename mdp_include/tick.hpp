// Tick struct // 11.06.26// ZeroK
// will be used by both client and server


#pragma once

#include <cstdint>

struct alignas(64) Tick {

    std::uint64_t seq;               // client needs sequence number to detect packet loss
    std::uint64_t timestamp_tsc;     // time when tick generated

    double bid;
    double ask;

    std::uint32_t bid_qty;
    std::uint32_t ask_qty;

    char symbol [8];
    char reserved [16];               // make the size of Tick to 64 byte

};

static_assert (sizeof(Tick) == 64, "WARNING: Tick should be 64 bytes.\n");
// sizeof(Tick) = 64 bytes
