#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return ticks - rec_tick; }

void TCPConnection::empty_ack_send() {
    // if(_sender.segments_out().empty() && !is_fin){
    if (_sender.segments_out().empty()) {
        // && !(_sender.fully_acked())){
        // send the ack
        if (_receiver.ackno().has_value()) {
            // cout << _receiver.ackno().value()<<endl;
            _sender.send_empty_ack_segment(_receiver.ackno().value());
        }
    }
}

void TCPConnection::segment_send() {
    // cout <<"empty:"<< _sender.segments_out().empty()<<endl;
    // cout<<"fin:"<<is_fin<<"   size:"<<_segments_out.size()<<"
    // ackno:"<<_segments_out.front().header().ackno.raw_value()<<endl;

    // sending segments
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (!seg.header().ack && _receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            // cout << _receiver.ackno().value()<<endl;
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
        // cout <<"segs size:"<<_segments_out.size()<< "   send a seg,length = "<<seg.length_in_sequence_space()<<"
        // ackno:"<<seg.header().ackno.raw_value()<<endl;
        // cerr<<"send a segment,";
        // seg.print_seg();
        _sender.segments_out().pop();
    }
}

void TCPConnection::set_rst() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    //cerr << "receive a seg,";
    //seg.print_seg();
    rec_tick = ticks;
    if (seg.header().rst) {
        // RST is set
        set_rst();
        return;
    }

    // if(_receiver.ackno().has_value())
    // cout << "seqno:"<<seg.header().seqno<<"   ackno:"<<_receiver.ackno().value()<<endl;
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno - _receiver.ackno().value() < 0) {
        _sender.send_empty_segment();
        segment_send();
        return;
    }
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    } else
        _sender.fill_window();
    if (seg.length_in_sequence_space() != 0) {
        empty_ack_send();
    }
    segment_send();
    is_fin = _sender.is_fin() && _receiver.is_fin();
    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const{
    if (!_linger_after_streams_finish && _receiver.stream_out().error() && _sender.stream_in().error()) {
        return false;
    }
    if (_receiver.stream_out().input_ended() &&
        _sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _sender.fully_acked()) {
        if (!_linger_after_streams_finish)
            return false;
        else if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout){
            return false;
	}
    }
    return true;
}

size_t TCPConnection::write(const string &data) {
    size_t res = _sender.stream_in().write(data);
    _sender.fill_window();
    segment_send();
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    ticks += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        // cout << "comehere!!cons:" << _sender.consecutive_retransmissions() << endl;
        while (!_sender.segments_out().empty())
            _sender.segments_out().pop();  // 清除超时重传的那个东西
        _sender.send_empty_rst_segment();
        set_rst();
    }
    segment_send();
    // end the connection cleanly if necessary
    if (_receiver.stream_out().input_ended() &&
        _sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _sender.fully_acked()
	&& time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
	    _linger_after_streams_finish = false;
    }

}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    segment_send();
}

void TCPConnection::connect() {
    _sender.fill_window();
    if (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (!seg.header().ack && _receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            // cout << "connect:"<<_receiver.ackno().value()<<endl;
        }
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
        // cout << "send a seg,length = "<<seg.length_in_sequence_space()<<endl;
        // cerr<<"send a segment,";
        // print_seg(seg);

        _sender.segments_out().pop();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            // cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // cout<< "go to destructor"<<endl;

            // Your code here: need to send a RST segment to the peer
            _sender.send_empty_rst_segment();
            segment_send();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
