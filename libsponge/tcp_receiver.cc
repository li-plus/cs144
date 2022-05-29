#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn) {
        _syn_recv = true;
        _isn = seg.header().seqno;
    }
    if (_syn_recv) {
        uint64_t index = unwrap(seg.header().seqno, _isn, _reassembler.first_unassembled());
        if (!seg.header().syn) {
            index--;  // absolute seqno should shift 1 since SYN takes 1 byte
        }
        _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recv) {
        return {};
    }
    uint64_t abs_seqno = _reassembler.first_unassembled() + 1;  // SYN takes 1 byte
    if (_reassembler.stream_out().input_ended()) {
        abs_seqno++;  // FIN takes 1 byte
    }
    return wrap(abs_seqno, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.first_unacceptable() - _reassembler.first_unassembled(); }
