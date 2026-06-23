// packet structure// 22.06.26// ZeroK

/* Ethernet frame:
 *      - Ethernet header
 *      - IPv4 header
 *      - UDP header
 *      - payload (Tick)
 * */

#pragma once

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "tick.hpp"

struct Packet {
    
    rte_ether_hdr   eth;
    rte_ipv4_hdr    ip;
    rte_udp_hdr     udp;
    Tick            tick;
};

static assert (sizeof(Packet) == 
                    sizeof(rte_ether_hdr) +
                    sizeof(rte_ipv4_hdr) +
                    sizeof(rte_udp_hdr) +
                    sizeof(Tick)
                );
