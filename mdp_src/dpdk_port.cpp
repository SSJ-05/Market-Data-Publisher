// dpdk_port cpp file// 22.06.26// ZeroK

/* this file contains
 * config constants
 * EAL init
 * H/W check
 * port configure
 * mempool init
 * rx/tx queue setup
 * start port
 * shutdown (stop port, close port, eal cleanup)
 * */


#include "dpdk_port.hpp"

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include <cstdio>


namespace cfg {

    constexpr std::uint16_t  TX_DESC      { 1 << 12 };  // 4096
    constexpr std::uint16_t  RX_DESC      { 1 << 10 };  // 1024
    constexpr std::uint16_t  NUM_MBUFS    { 1 << 13 };  // 8192
    constexpr std::uint16_t  CACHE_SIZE   { 1 << 8 };   // 256
    constexpr std::uint16_t  RX_Q         { 1 };
    constexpr std::uint16_t  TX_Q         { 1 };
    constexpr std::uint16_t  PORT_ID      { 0 };

    // static_assert (NUM_MBUFS >= 2 * TX_DESC, "INCREASE NUM_MBUFS.\n");
}


// EAL init
static int eal_init (int argc, char** argv) {

    int ret;
    if ((ret = rte_eal_init (argc, argv)) < 0)
        rte_exit (EXIT_FAILURE, "EAL INIT FAILED\n");

    return ret;
}


// check H/W
static std::uint16_t discover_port () {
    
    unsigned ports = rte_eth_dev_count_avail();
    
    if (ports == 0) rte_exit (EXIT_FAILURE, "NO PORTS AVAILABLE.\n");

    if (ports != 1)
        std::printf ("\nPorts available: %u, "  "We use port %d\n", 
                        ports, cfg::PORT_ID);


    rte_eth_dev_info info;
    int ret = rte_eth_dev_info_get (cfg::PORT_ID, &info); 
    if (ret < 0) rte_exit (EXIT_FAILURE, "FAILED TO GET ETH DEV INFO.\n");
    std::printf ("\nAVAILABLE H/W:\n"
                    "\tports: %u\n"
                    "\ttx_queues: %u\n"
                    "\trx_queues: %u\n"
                    "\tdriver_name: %s\n",
                    ports, info.max_tx_queues, info.max_rx_queues, info.driver_name
                );

    return cfg::PORT_ID;
}


// create mempool
static rte_mempool* create_mempool (std::uint16_t buf_size,
                                    std::uint16_t cache_size, 
                                    const char* name) {
    
    auto* pool = 
        rte_pktmbuf_pool_create (
                name,
                buf_size,
                cache_size,
                0,
                RTE_MBUF_DEFAULT_BUF_SIZE,
                rte_socket_id()
            );
    if (!pool) rte_exit (EXIT_FAILURE, "POOL CREATION FAILED\n");

    return pool;
}


// create RX Queue and rx_pool
static rte_mempool* create_rx_pool_q (std::uint16_t desc_sz) {
    
    auto* rx_pool = 
        create_mempool (cfg::NUM_MBUFS, cfg::CACHE_SIZE, "rx_pool");
    
    int ret = 
        rte_eth_rx_queue_setup (
            cfg::PORT_ID,
            0,
            desc_sz,
            rte_socket_id(),
            nullptr,
            rx_pool
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "RX QUEUE CREATION FAILED\n");

    return rx_pool;
}


// create TX Queue
static void create_tx_q (std::uint16_t desc_sz) {
    
    int ret = 
        rte_eth_tx_queue_setup (
            cfg::PORT_ID,
            0,
            desc_sz,
            rte_socket_id(),
            nullptr
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "TX QUEUE CREATION FAILED\n");
}


// configure port
static void port_config () {
    
    rte_eth_conf port_conf {};
    int ret = 
        rte_eth_dev_configure (
            cfg::PORT_ID,
            cfg::RX_Q,              // no. of rx queues
            cfg::TX_Q,              // no. of tx queues
            &port_conf
        );
    if (ret < 0) rte_exit (EXIT_FAILURE, "PORT CONFIGURE FAILED\n");
}



// dev start port
static void start_port () {

    int ret = rte_eth_dev_start (cfg::PORT_ID);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV START FAILED\n");

    rte_delay_ms (3000);

    rte_eth_link link {};

    ret = rte_eth_link_get (cfg::PORT_ID, &link);
    if (ret < 0) rte_exit (EXIT_FAILURE, "LINK QUERY FAILED\n");

    std::printf("\nret : %d "
        "link_status = %s  speed = %u  duplex = %u\n",
        ret,
        link.link_status ? "UP" : "DOWN",
        link.link_speed,
        link.link_duplex
    );

    rte_eth_promiscuous_enable(cfg::PORT_ID);    // accept all frames
                                            // for dev phase only - remove later
}



// stop dev, close port, cleanup EAL
void dpdk::shutdown (const Port& port) {
    
    int ret = rte_eth_dev_stop (port.port_id);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV STOP FAILED\n");

    ret = rte_eth_dev_close (port.port_id);
    if (ret < 0) rte_exit (EXIT_FAILURE, "ETH DEV CLOSE FAILED\n");

    ret = rte_eal_cleanup();
    if (ret < 0) rte_exit (EXIT_FAILURE, "RTE EAL CLEANUP FAILED\n");
}




// port init
// bring dpdk online - eal init
// discover port
// configure port
// create mempools
// create tx/rx queues
// start port
dpdk::Port dpdk::init (int argc, char** argv) {

    eal_init (argc, argv);

    auto port_id = discover_port ();

    port_config ();

    auto* rx_pool = create_rx_pool_q (cfg::RX_DESC);
    auto* tx_pool = create_mempool (
                            cfg::NUM_MBUFS,
                            cfg::CACHE_SIZE,
                            "tx_pool");
    create_tx_q (cfg::TX_DESC);

    start_port ();

    return dpdk::Port {
             .port_id = port_id,
             .tx_pool = tx_pool,
             .rx_pool = rx_pool 
            };
}
