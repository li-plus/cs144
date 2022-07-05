#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    uint32_t next_hop_ip = next_hop.has_value() ? next_hop->ipv4_numeric() : 0;
    _route_table.emplace_back(RouteEntry(route_prefix, prefix_length, next_hop_ip, interface_num));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    const RouteEntry *lpm_route = nullptr;  // longest-prefix-match
    for (const auto &route : _route_table) {
        uint32_t mask = -(uint64_t(1) << (32 - route.prefix_length));
        if ((dgram.header().dst & mask) == route.route_prefix) {
            if (!lpm_route || lpm_route->prefix_length < route.prefix_length) {
                lpm_route = &route;
            }
        }
    }
    if (lpm_route) {
        if (dgram.header().ttl > 1) {
            dgram.header().ttl--;
            uint32_t next_hop_ip = (lpm_route->next_hop_ip == 0) ? dgram.header().dst : lpm_route->next_hop_ip;
            interface(lpm_route->interface_num).send_datagram(dgram, Address::from_ipv4_numeric(next_hop_ip));
        } else {
            // ttl expired, may send ICMP time exceeded?
        }
    } else {
        // no route to host, may send ICMP destination unreachable?
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
