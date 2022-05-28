#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _unassembled_bytes(0)
    , _start_idx(0)
    , _eof_idx(-1)
    , _buffer(capacity, '\0')
    , _bitmap(capacity, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_idx = index + data.size();
    }

    // read data into buffer
    size_t min_idx = std::max(index, _start_idx);
    size_t max_idx = std::min(index + data.size(), _start_idx + _capacity);
    for (size_t idx = min_idx; idx < max_idx; idx++) {
        size_t buf_idx = idx % _capacity;
        if (!_bitmap[buf_idx]) {
            _buffer[buf_idx] = data[idx - index];
            _bitmap[buf_idx] = true;
            _unassembled_bytes++;
        }
    }

    // write assembled bytes into output
    std::string out_string;
    for (size_t i = 0; i < std::min(_capacity, _output.remaining_capacity()); i++) {
        size_t buf_idx = _start_idx % _capacity;
        if (!_bitmap[buf_idx]) {
            break;
        }
        out_string.append(1, _buffer[buf_idx]);
        _bitmap[buf_idx] = false;
        _unassembled_bytes--;
        _start_idx++;
    }
    _output.write(out_string);

    if (_start_idx == _eof_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
