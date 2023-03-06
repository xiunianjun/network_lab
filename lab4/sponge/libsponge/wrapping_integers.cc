#include "wrapping_integers.hh"

#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint32_t tmp = (n & TAIL_MASK);
    WrappingInt32 res = isn + tmp;
    // if(n == 2)
    // cout << "wrap:"<<res.raw_value()<<endl;
    return res;
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t tmp_n = n.raw_value() - isn.raw_value();
    uint64_t res = (checkpoint & HEAD_MASK);
    uint32_t tmp_cp = (checkpoint & TAIL_MASK);
    // cp在中间的话直接或就行了
    // cp在左边，则当tmp_n小于cp，直接或，大于cp，则当它大于cp+FLAG时，需要-HEAD_ONE否则不用
    res |= tmp_n;
    if (tmp_cp < FLAG) {
        if (tmp_n > tmp_cp + FLAG) {
            if (res >= HEAD_ONE)
                res -= HEAD_ONE;
        }
    } else if (tmp_cp > FLAG) {
        if (tmp_n < tmp_cp - FLAG)
            res += HEAD_ONE;
    }
    // if(n.raw_value() == 2)
    // cout << "unwrap:"<<res<<endl;
    return res;
}
