// dpdk_port header file// 22.06.26 // ZeroK

#pragma once

#include <cstdint>

struct rte_mempool;

namespace dpdk {

    struct Port {

        std::uint16_t   port_id     {};
        rte_mempool*    tx_pool     {};
        rte_mempool*    rx_pool     {};
    };

    Port init (int argc, char** argv);
    // std::uint16_t init (int argc, char** argv);

    void shutdown (Port& port);
    // void shutdown (std::uint16_t port_id);

} // namespace dpdk
