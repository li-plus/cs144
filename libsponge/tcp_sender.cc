#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    return std::accumulate(_outstanding_segments.begin(),
                           _outstanding_segments.end(),
                           size_t(0),
                           [](size_t sum, const TCPSegment &seg) { return sum + seg.length_in_sequence_space(); });
}

void TCPSender::fill_window() {
    auto send_non_empty_segment = [this](const TCPSegment &seg) {
        _segments_out.push(seg);
        _outstanding_segments.push_back(seg);
        _next_seqno += seg.length_in_sequence_space();
    };
    size_t window_size = std::max(_window_size, size_t(1));
    if (_next_seqno == 0) {
        // CLOSED
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = next_seqno();
        send_non_empty_segment(seg);
    } else if (_next_seqno == bytes_in_flight()) {
        // SYN_SENT
    } else if (!stream_in().eof()) {
        // SYN_ACKED
        size_t avail_size = window_size - (_next_seqno - _ackno);
        while (avail_size > 0 && !_stream.buffer_empty()) {
            size_t payload_size = std::min({_stream.buffer_size(), avail_size, TCPConfig::MAX_PAYLOAD_SIZE});
            TCPSegment seg;
            seg.header().seqno = next_seqno();
            seg.payload() = _stream.read(payload_size);
            if (_stream.eof() && avail_size > payload_size) {
                seg.header().fin = true;
            }
            send_non_empty_segment(seg);
            avail_size -= seg.length_in_sequence_space();
        }
    } else if (_next_seqno < stream_in().bytes_written() + 2) {
        // SYN_ACKED
        if (window_size > _next_seqno - _ackno) {
            TCPSegment seg;
            seg.header().seqno = next_seqno();
            seg.header().fin = true;
            send_non_empty_segment(seg);
        }
    } else if (bytes_in_flight()) {
        // FIN_SENT
    } else {
        // FIN_ACKED
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ackno_abs = unwrap(ackno, _isn, _next_seqno);
    if (_ackno <= ackno_abs && ackno_abs <= _next_seqno) {
        // valid ackno
        if (_ackno < ackno_abs) {
            // new data is acknowledged
            _ackno = ackno_abs;
            auto ack_pos = std::find_if(
                _outstanding_segments.begin(), _outstanding_segments.end(), [this, ackno_abs](const TCPSegment &seg) {
                    uint64_t segno_abs = unwrap(seg.header().seqno, _isn, _next_seqno);
                    return ackno_abs < segno_abs + seg.length_in_sequence_space();
                });
            _outstanding_segments.erase(_outstanding_segments.begin(), ack_pos);

            // reset retransmission metrics
            _timer.set_rto(_initial_retransmission_timeout);
            _timer.reset();
            _consecutive_retransmissions = 0;
        }
        _window_size = window_size;
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    bool timeout = _timer.tick(ms_since_last_tick);
    if (timeout) {
        if (!_outstanding_segments.empty()) {
            _segments_out.push(_outstanding_segments.front());
            if (_window_size > 0) {
                _consecutive_retransmissions++;
                _timer.set_rto(_timer.rto() * 2);  // exponential backoff
            }
        }
        _timer.reset();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
