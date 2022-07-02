#include "stream_reassembler.hh"

#include <algorithm>

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

    uint64_t min_idx = std::max(index, _start_idx);
    uint64_t max_idx = std::min(index + data.size(), _start_idx + _output.remaining_capacity());
    if (min_idx < max_idx) {
        // read data into buffer
        std::copy(data.begin() + (min_idx - index),
                  data.begin() + (max_idx - index),
                  _buffer.begin() + (min_idx - _start_idx));
        std::fill_n(_bitmap.begin() + (min_idx - _start_idx), max_idx - min_idx, true);

        // write assembled bytes into output
        auto write_size = std::find(_bitmap.begin(), _bitmap.end(), false) - _bitmap.begin();
        std::string out_string(_buffer.begin(), _buffer.begin() + write_size);
        _output.write(out_string);
        _buffer.erase(_buffer.begin(), _buffer.begin() + write_size);
        _buffer.insert(_buffer.end(), write_size, '\0');
        _bitmap.erase(_bitmap.begin(), _bitmap.begin() + write_size);
        _bitmap.insert(_bitmap.end(), write_size, false);
        _start_idx += write_size;
    }

    if (_start_idx == _eof_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return std::count(_bitmap.begin(), _bitmap.end(), true); }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
