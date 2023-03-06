#include "byte_stream.hh"

#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t cap)
    : total_write(0), total_read(0), is_input_end(false), capacity(cap), buffer() {}

//! Write a string of bytes into the stream. Write as many
//! as will fit, and return how many were written.
//! \returns the number of bytes accepted into the stream
size_t ByteStream::write(const string &data) {
    // if (is_input_end == true)
    //    is_input_end = false;
    int pointer = 0;
    // int length = min(data.length(),capacity-buffer.size());
    int length = data.length();
    // while (is_input_end == false && pointer < length) {
    while (pointer < length) {
        // while(buffer.size() == capacity);
        if (buffer.size() == capacity)
            break;
        buffer.push_back(data[pointer]);
        pointer++;
    }
    total_write += pointer;
    return pointer;
}

//! Peek at next "len" bytes of the stream
//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    size_t i = 0;
    for (auto it = buffer.begin(); it != buffer.end(); it++) {
        if (i >= len)
            break;
        i++;
        res.push_back(*it);
    }
    return res;
}

//! Remove bytes from the buffer
//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        if (buffer.empty())
            break;
        buffer.pop_front();
    }
    total_read += i;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { is_input_end = true; }

bool ByteStream::input_ended() const { return is_input_end; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return is_input_end && buffer.empty(); }

size_t ByteStream::bytes_written() const { return total_write; }

size_t ByteStream::bytes_read() const { return total_read; }

size_t ByteStream::remaining_capacity() const {
    if (capacity > buffer.size())
        return capacity - buffer.size();
    else
        return 0;
}
