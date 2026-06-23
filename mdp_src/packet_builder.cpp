// build packets// 22.06.26// ZeroK

#include "packet_builder.hpp"
#include "tick.hpp"
#include "packet.hpp"

Packet build_pkt (const Tick& _tick) {
    
    Packet pkt {};
    pkt.tick = _tick;

    // fill udp
    pkt.udp.src_port  = rte_cpu_to_be_16 (9000);
    
    pkt.udp.dst_port  = rte_cpu_to_be_16 (9001);

    pkt.udp.dgram_len = rte_cpu_to_be_16 (sizeof(rte_udp_hdr) + sizeof(Tick));


    // fill ipv4
    pkt.ip.version_ihl   = 0x45;

    pkt.ip.time_to_live  = 64;

    pkt.ip.next_proto_id = IPPROTO_UDP;

    pkt.ip.total_length  = rte_cpu_to_be_16 (sizeof(rte_ipv4_hdr) +
                                             sizeof(rte_udp_hdr) +
                                             sizeof(Tick));

    // fill ethernet
    rte_ether_addr src {};
    rte_ether_addr dst {};

    return pkt;
}
