#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_syn_recv) {  // CLOSED
        if (seg.header().syn) {
            _syn_recv = true;
            _isn = seg.header().seqno;
            _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        }
    } else if (!stream_out().input_ended()) {  // SYN_RECV
        uint64_t index = unwrap(seg.header().seqno, _isn, _reassembler.first_unassembled()) - 1;
        _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
    } else {  // FIN_RECV
        // ignore
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn_recv) {  // CLOSED
        return {};
    } else if (!stream_out().input_ended()) {  // SYN_RECV
        return wrap(_reassembler.first_unassembled() + 1, _isn);
    } else {  // FIN_RECV
        return wrap(_reassembler.first_unassembled() + 2, _isn);
    }
}

size_t TCPReceiver::window_size() const { return _reassembler.first_unacceptable() - _reassembler.first_unassembled(); }
