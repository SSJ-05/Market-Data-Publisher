// market data publisher// 22.06.26// ZeroK
// business logic here


/* this file contains
 * ring.pop()
 * header template pre built
 * burst assembly
 * mbuf alloc
 * tx_burst
 * stats
 * */


#include "publisher.hpp"
#include "global_shutdown.hpp"

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <immintrin.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include <atomic>
#include <cstdint>
#include <cstddef>  // for std::byte
#include <cstdio>




// counters for stats
namespace counters {

    alignas(64) std::atomic<std::size_t> sent_pkts {};
    // char pad_0 [64 - sizeof(std::atomic<std::size_t>)];

    alignas(64) std::atomic<std::size_t> dropped_pkts {};
    // char pad_1 [64 - sizeof(std::atomic<std::size_t>)];

    std::atomic<uint64_t> total_bursts{};
    std::atomic<uint64_t> total_pkts{};

    
    void get_publisher_stats () noexcept {
        std::printf ("sent: %zu\n" "dropped: %zu\n\n",
                sent_pkts.load (std::memory_order_relaxed),
                dropped_pkts.load (std::memory_order_relaxed));
    }
}   // namespace counters



// prebuild header template - only copy Tick into payload
struct Hdr_template {

    rte_ether_hdr  eth;
    rte_ipv4_hdr   ip;
    rte_udp_hdr    udp;
};
static_assert (sizeof(Hdr_template) == 42);
static_assert (cfg::PKT_SIZE == sizeof(Hdr_template) + sizeof(Tick));


// helper func - header builder - private to this file
// prebuild headers - then memcpy payload into mbuf
static Hdr_template build_template() {
   
    Hdr_template hdr {};

    // fill udp
    hdr.udp.src_port  = rte_cpu_to_be_16 (9000);
    
    hdr.udp.dst_port  = rte_cpu_to_be_16 (9001);

    hdr.udp.dgram_len = rte_cpu_to_be_16 (sizeof(rte_udp_hdr) + sizeof(Tick));


    // fill ipv4
    hdr.ip.version_ihl   = 0x45;

    hdr.ip.time_to_live  = 64;

    hdr.ip.next_proto_id = IPPROTO_UDP;

    hdr.ip.total_length  = rte_cpu_to_be_16 (sizeof(rte_ipv4_hdr) +
                                             sizeof(rte_udp_hdr) +
                                             sizeof(Tick));
    
    hdr.ip.hdr_checksum  = rte_ipv4_cksum (&hdr.ip);
    
    // fill ethernet
    // rte_ether_addr src {};
    // rte_ether_addr dst {};
    
    hdr.eth.ether_type = rte_cpu_to_be_16 (RTE_ETHER_TYPE_IPV4);
    
    return hdr;
}


// helper func - burst builder - private to this file
// A A M M - Alloc Append Mtod Memcpy
[[ nodiscard ]]
static std::uint16_t build_burst (  rte_mempool* pool,
                                    const Hdr_template& hdr,
                                    Tick ticks[],          
                                    rte_mbuf* pkts[],       
                                    std::uint16_t count ) {

    std::uint16_t built {};

    for (; built < count; ++built) {

        auto* pkt = rte_pktmbuf_alloc (pool);
        if (!pkt) break;

        void* dst = rte_pktmbuf_append (pkt, cfg::PKT_SIZE);
        if (!dst) {
            rte_pktmbuf_free (pkt);
            break;
        }

        auto* eth = rte_pktmbuf_mtod (pkt, rte_ether_hdr*);
        
        __builtin_memcpy (eth, &hdr, sizeof(Hdr_template));


        // // nested cast - intent: this memory is both tck and eth
        // // cast 8bit int ptr on eth
        // // move the eth ptr forward by Hdr_template size
        // // cast Tick ptr to the new mem location
        // acceptable for trivially copyable obj
        auto* tck = reinterpret_cast<Tick*>(
                            reinterpret_cast<std::uint8_t*>(eth)
                            + sizeof(Hdr_template));
        // // or 
        // auto* payload = reinterpret_cast<std::byte*>(eth) + sizeof(Hdr_template);
        // Tick* tck = reinterpret_cast<Tick*>(payload);

        *tck = ticks[built];

        pkts[built] = pkt;
    }

    return built;
}



struct alignas(64) burst_context {

    Tick ticks[cfg::BURST_SIZE];        
    rte_mbuf* pkts[cfg::BURST_SIZE];   

    Hdr_template hdr {};
};


void publisher::run (dpdk::Port& port,
                     zerok::z_ring<Tick, (1 << 16)>& ring) {

    burst_context bc;
    bc.hdr = build_template();

    while (g_RUNNING || !ring.empty()) {
        
        std::uint16_t count {};
        while (count < cfg::BURST_SIZE) {
            if (!ring.pop (bc.ticks[count]))
                break;

            ++count;
        }

        auto built = 
            build_burst (
                port.tx_pool,
                bc.hdr,
                bc.ticks,
                bc.pkts,
                count
            );

        if (built == 0) {
            _mm_pause();
            continue;
        }

        auto sent =
            rte_eth_tx_burst (
                    port.port_id,
                    0,
                    bc.pkts,
                    built
                );


        for (auto i {sent}; i < built; ++i) {
            rte_pktmbuf_free (bc.pkts[i]);
        }


        // // cleanup tx queue
        // std::uint16_t cleaned =
        //     rte_eth_tx_done_cleanup (
        //             port.port_id,
        //             0, 
        //             64 
        //         );
        // if (cleaned) 
        //     std::printf ("cleaned : %u\n", cleaned);

        // counters::total_bursts.fetch_add(1);
        // counters::total_pkts.fetch_add(built);

        // increment counters
        counters::sent_pkts.fetch_add (sent, std::memory_order_relaxed);
        counters::dropped_pkts.fetch_add (built - sent, std::memory_order_relaxed);

    }
    
}




