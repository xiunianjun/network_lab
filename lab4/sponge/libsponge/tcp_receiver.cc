#include "tcp_receiver.hh"

#include <iostream>
#include <stdlib.h>
#include <time.h>

#define random(x) (rand() % x)

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    WrappingInt32 seqno = header.seqno;
    string data = seg.payload().copy();
    size_t index = 0;          // the param of the reassembler
    if (!syn && header.syn) {  // is the first packet
        // 接收到连接建立请求报文，初始化ackno和isn
        _reassembler.set_is_eof();
        fin = false;
        isn = header.seqno;
        seqno = seqno + 1;  // plus one to skip the SYN byte
        syn = true;
    }
    if (header.fin)
        fin = true;
    if (syn) {
        // 转换index
        uint64_t abs_seqno = unwrap(seqno, isn, ack);
        index = abs_seqno - 1;
        // 写入整流器
        //cout << "index:  " << index << "  data:" << data.length() << endl;
        // if (abs_seqno != 0 && index < _reassembler.get_left_bound() + window_size())
        if (abs_seqno != 0)
            _reassembler.push_substring(data, index, header.fin);
        ack = _reassembler.get_left_bound() + 1;
        // cout << "empty?"<<_reassembler.empty()<<"  ack?"<<ack<<"  fin?"<<fin<<endl;
        if (_reassembler.empty() && fin) {
            // if(header.fin){
            ack += 1;
        }
        // cout <<"ack:" << ack<<endl;
    }
}
optional<WrappingInt32> TCPReceiver::ackno() const {
    // cout <<"ack:"<<ack<<"   isn:"<<isn.raw_value()<<endl;
    if (syn)
        return {wrap(ack, isn)};
    else
        return {};
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
