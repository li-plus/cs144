#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

constexpr size_t ARP_REQUEST_INTERVAL = 5 * 1000;

static EthernetFrame make_arp_frame(uint16_t opcode,
                                    const EthernetAddress &sender_mac,
                                    uint32_t sender_ip,
                                    const EthernetAddress &target_mac,
                                    uint32_t target_ip) {
    ARPMessage arp_message;
    arp_message.opcode = opcode;
    arp_message.sender_ethernet_address = sender_mac;
    arp_message.sender_ip_address = sender_ip;
    arp_message.target_ethernet_address = (opcode == ARPMessage::OPCODE_REPLY) ? target_mac : EthernetAddress{};
    arp_message.target_ip_address = target_ip;

    EthernetFrame frame;
    frame.header().dst = (opcode == ARPMessage::OPCODE_REPLY) ? target_mac : ETHERNET_BROADCAST;
    frame.header().src = sender_mac;
    frame.header().type = EthernetHeader::TYPE_ARP;
    frame.payload() = arp_message.serialize();

    return frame;
}

static EthernetFrame make_ip_frame(const InternetDatagram &dgram,
                                   const EthernetAddress &dst_mac,
                                   const EthernetAddress &src_mac) {
    EthernetFrame frame;
    frame.header().dst = dst_mac;
    frame.header().src = src_mac;
    frame.header().type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();
    return frame;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto arp_entry = _arp_table.find(next_hop_ip);
    if (arp_entry == _arp_table.end()) {
        // mac address not found. need to send arp request
        if (_ms_clock - _ms_last_arp > ARP_REQUEST_INTERVAL) {
            _frames_out.push(make_arp_frame(ARPMessage::OPCODE_REQUEST,
                                            _ethernet_address,
                                            _ip_address.ipv4_numeric(),
                                            EthernetAddress{},
                                            next_hop_ip));
            _ms_last_arp = _ms_clock;
        }
        _pending_dgrams[next_hop_ip].emplace_back(dgram);
    } else {
        auto frame = make_ip_frame(dgram, arp_entry->mac, _ethernet_address);
        _frames_out.push(frame);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address) {
        return {};
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ipv4_datagram;
        ipv4_datagram.parse(frame.payload());
        return ipv4_datagram;
    }
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_message;
        arp_message.parse(frame.payload());
        // learn arp mapping
        _arp_table.push_back(ArpEntry(arp_message.sender_ip_address, arp_message.sender_ethernet_address, _ms_clock));
        if (arp_message.opcode == ARPMessage::OPCODE_REQUEST &&
            arp_message.target_ip_address == _ip_address.ipv4_numeric()) {
            _frames_out.push(make_arp_frame(ARPMessage::OPCODE_REPLY,
                                            _ethernet_address,
                                            _ip_address.ipv4_numeric(),
                                            arp_message.sender_ethernet_address,
                                            arp_message.sender_ip_address));
        }
        auto pending_dgrams_it = _pending_dgrams.find(arp_message.sender_ip_address);
        if (pending_dgrams_it != _pending_dgrams.end()) {
            for (const auto &dgram : pending_dgrams_it->second) {
                _frames_out.push(make_ip_frame(dgram, arp_message.sender_ethernet_address, _ethernet_address));
            }
            _pending_dgrams.erase(pending_dgrams_it);
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _ms_clock += ms_since_last_tick;
    while (!_arp_table.empty() && _ms_clock - _arp_table.front().ms_inserted > ArpTable::TTL) {
        _arp_table.pop_front();
    }
}
