// dpdk_port header file// 22.06.26 // ZeroK

#pragma once

#include <cstdint>

// fwd declaration
struct rte_mempool;

namespace dpdk {

    struct Port {

        std::uint16_t   port_id     {};
        rte_mempool*    tx_pool     {};
        rte_mempool*    rx_pool     {};
    };

    [[ nodiscard ]]
    Port init (int argc, char** argv);

    void shutdown (const Port& port);

} // namespace dpdk
