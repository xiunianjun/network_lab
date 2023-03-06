#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , rto{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const { return tmp_size; }

// 尽可能地创造segment并且填充到segment output中
void TCPSender::fill_window() {
    // should act like the window size is one
    size_t t_win_size = window_size == 0 ? 1 : window_size;
    size_t remaining = t_win_size - tmp_size;
    // 防止数值溢出的情况
    if (t_win_size < tmp_size)
        remaining = 0;

    // fill as possible
    while (remaining > 0) {
        // create and fill in a segment
        TCPSegment segment = TCPSegment();
        if (!syn) {
            // first segment
            segment.header().syn = true;
            segment.header().seqno = _isn;
            remaining -= 1;
            syn = true;
            // should start the timer here
            timer_start = true;
            timer_ticks = ticks;
        }
        // fill in the payload
        if (!segment.header().syn && !(_stream.buffer_empty() || remaining == 0)) {
            string data = _stream.read(min(remaining, TCPConfig::MAX_PAYLOAD_SIZE));
            remaining -= data.length();
            Buffer buf = Buffer(move(data));
            segment.payload() = buf;
        }

        if (_stream.input_ended() && !fin && remaining > 0) {
            // last segment
            segment.header().fin = true;
            fin = true;
            remaining -= 1;
        }

        // segment为空（不为SYN、FIN，也不携带任何数据）
        if (segment.length_in_sequence_space() == 0)
            break;

        segment.header().seqno = wrap(_next_seqno, _isn);
        _next_seqno += segment.length_in_sequence_space();
        // push into the outstanding segments
        tmp_segments.push_back(
            {segment, unwrap(segment.header().seqno, _isn, _next_seqno), segment.length_in_sequence_space()});
        tmp_size += segment.length_in_sequence_space();
        // push into the segment out queue
        _segments_out.push(segment);
    }
}

void TCPSender::ack_received(const WrappingInt32 ack, const uint16_t wind_size) {
    window_size = wind_size;
    uint64_t a_ack = unwrap(ack, _isn, ackno);
    if (a_ack > _next_seqno)
        return;  // impossible ack is ignored
    if (a_ack > ackno) {
        // reset the retransmission
        rto = _initial_retransmission_timeout;
        timer_ticks = ticks;
        cons_retran = 0;
        // erase elements from the tmp_segments
        for (auto it = tmp_segments.begin(); it != tmp_segments.end();) {
            if (a_ack >= it->seqno + it->data_size) {
                tmp_size -= (it->segment).length_in_sequence_space();
                // 如果FIN报文被成功接收，就关闭timer
                if (it->segment.header().fin)
                    timer_start = false;
                it = tmp_segments.erase(it);
            } else
                it++;
        }
    }
    ackno = a_ack;
    fill_window();
}

void TCPSender::tick(const size_t ms_since_last_tick) {
    if (ticks > ticks + ms_since_last_tick) {
        // 进行简单的溢出处理，还是有可能溢出
        ticks -= timer_ticks;
        timer_ticks = 0;
    }
    ticks += ms_since_last_tick;

    if (timer_start && ticks > timer_ticks && ticks - timer_ticks >= rto) {
        if (!tmp_segments.empty()) {
            // resend
            _segments_out.push(tmp_segments.front().segment);

            if (window_size != 0) {
                cons_retran++;
		cerr << "cons_retran:"<<cons_retran<<endl;
                rto *= 2;
            }
            //cout << "retran:" << cons_retran << "   ticks:" << ticks << "  timer_ticks:" << timer_ticks
             //    << "   rto:" << rto << endl;
        }
        timer_ticks = ticks;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return cons_retran; }

void TCPSender::send_empty_segment() {
    TCPSegment segment = TCPSegment();
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}

void TCPSender::send_empty_ack_segment(WrappingInt32 t_ackno) {
    TCPSegment segment = TCPSegment();
    segment.header().seqno = wrap(_next_seqno, _isn);
    segment.header().ack = true;
    segment.header().ackno = t_ackno;
    // cout <<"t_ackno = "<<t_ackno<<endl;
    _segments_out.push(segment);
}

void TCPSender::send_empty_rst_segment() {
    TCPSegment segment = TCPSegment();
    segment.header().seqno = wrap(_next_seqno, _isn);
    segment.header().rst = true;
    _segments_out.push(segment);
}
