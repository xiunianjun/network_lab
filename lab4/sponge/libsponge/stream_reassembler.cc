#include "stream_reassembler.hh"

#include <iostream>
#include <map>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : is_eof(false), left_bound(0), buffer(), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // cout<<"eof: "<<is_eof<<" cur_cap:"<<data.length() + unassembled_bytes()<<endl;
    if (eof == 1)
        is_eof = true;

    // 为了实现buffer的容量限制
    size_t unass = unassembled_bytes();

    size_t remaining = _capacity > unass ? _capacity - unass : 0;  // how much room left?

    // if(remaining <= 0)	return; // buffer满
    size_t left = index, right = index + data.length();  // 初始化左右区间
    size_t o_left = left, o_right = right;               // keep the original value
    auto iterator = buffer.begin();                      // 这些变量在这里声明是为了防止后面goto报错
    node tmp = {0, 0, ""};
    // bool merged = false;

    if (right < left_bound)
        return;                                    // must be duplicated
    left = left < left_bound ? left_bound : left;  // 左边已经接受过的数据就不要了
    right = right <= left_bound + _capacity ? right : left_bound + _capacity;  // 右边越界的也不要
    o_right = right;
    string res = data.substr(left - o_left, right - left);

    // 用来防止buffer满
    size_t start = left;  // the begin of the unused data-zone of variable data
    if (data.compare("") == 0 || res.compare("") == 0)
        goto end;  // 如果是空串也直接不要
    if (o_left >= left_bound + _capacity)
        goto end;  // 越界的不要
    // if(remaining == 0)	goto end; // buffer满

    // 开始区间合并。需要扫描两次
    for (auto it = buffer.begin(); it != buffer.end(); it++) {
        if (left >= it->left && left <= it->right) {
            size_t r = right;
            size_t l = left;
            right = max(right, it->right);
            left = min(left, it->left);
            if (right == it->right) {
                start = o_right;
            } else {
                start = it->right;
            }
            // cout << "left: "<<left<<" r:"<<r<<" itleft:"<<it->left<<" itright:"<<it->right<<" oleft:"<<o_left<<endl;
            if (r <= it->right) {
                // cout << "1:"<<left-o_left<<" "<<r-it->left<<endl;
                res = it->data.substr(0, l - it->left) + data.substr(l - o_left) +
                      it->data.substr(r - it->left, it->right - r);
            } else {
                // res = it->data + data.substr(it->right-o_left);
                // res = it->data + data.substr(it->right-o_left,right-it->right);
                // cout << "2:"<<left-o_left<<endl;
                res = it->data.substr(0, l - it->left) + data.substr(l - o_left);
            }
            // 删除这一步很关键。
            buffer.erase(it);
            break;
        }
    }

    // 第二次
    for (auto it = buffer.begin(); it != buffer.end(); it++) {
        if (it->left >= left && it->left <= right) {
            size_t need = min(it->left - start, o_right - start);
            // 此时空间不够，先尝试下能不能写入一部分到_output中
            if (it->left > start && remaining < need) {
                if (left == left_bound) {
                    size_t out_mem = _output.remaining_capacity();
                    if (out_mem > 0) {
                        if (out_mem >= need) {
                            // 写入ByteStream
                            _output.write(res.substr(0, need));
                            // 更新
                            res = res.substr(need);
                            // left_bound = it->left - start + left;
                            left_bound += need;
                            left = left_bound;
                            remaining += need;
                            // remaining = 0;
                            goto ok;
                        } else {
                            _output.write(res.substr(0, out_mem));
                            res = res.substr(out_mem);
                            left_bound += out_mem;
                            left = left_bound;
                            remaining += out_mem;
                            if (it->left > start && remaining >= need) {
                                goto ok;
                            }
                        }
                    }
                }
                tmp = {left, start + remaining, res.substr(0, start - left + remaining)};
                remaining = 0;
                if (eof == 1)
                    is_eof = false;
                if (left < start + remaining)
                    buffer.insert(tmp);
                goto end;
            }
        ok:
            if (it->left > start)
                remaining -= need;
            start = it->right;
            if (it->right <= right)
                ;
            else {
                res += it->data.substr(right - it->left, it->right - right);
                right = it->right;
            }
            buffer.erase(it);
        }
    }

    if (start < o_right && remaining < o_right - start) {
        right = start + remaining;
        if (eof == 1)
            is_eof = false;
    }
    tmp = {left, right, res};
    if (left < right)
        buffer.insert(tmp);

end:
    // cout <<"haha2"<<endl;
    iterator = buffer.begin();
    // write into the ByteStream
    if (iterator != buffer.end() && iterator->left == left_bound) {
        // 防止_output的容量超过
        size_t out_rem = _output.remaining_capacity();
        if (out_rem < iterator->data.length()) {
            _output.write(iterator->data.substr(0, out_rem));
            left_bound = iterator->left + out_rem;
            tmp = {left_bound, iterator->right, iterator->data.substr(out_rem)};
            if (left_bound < iterator->right)
                buffer.insert(tmp);
            buffer.erase(iterator);
        } else {
            _output.write(iterator->data);
            left_bound = iterator->right;
            buffer.erase(iterator);
        }
    }

    // 调试用
    if (res.length() < 20) {
        // if(!buffer.empty())
        //    cout << "buffer: " << (buffer.begin())->left<<" "<<(buffer.begin())->right<<"
        //    "<<(buffer.begin())->data<<endl;
        // cout << "left_bound : "<<left_bound<<endl;
        // cout << "left: " << left << " right: " << right <<" res: "+res+"  data: "+data << endl;
        // cout << "_output:"<<_output.remaining_capacity()<<endl;
        // cout << "----------------------" << endl;
    }
    // 满足两个条件才是真的eof
    if (is_eof && buffer.empty()) {
        // is_eof = false;
        // left_bound ++;
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    // 可以跟上面的write合起来的，但是我暂时懒得搞
    size_t res = 0;
    for (auto it = buffer.begin(); it != buffer.end(); it++) {
        res += it->right - it->left;
        //if (res == 1)
            //cout << "r" << it->right << " l" << it->left << endl;
    }
    return res;
}

// bool StreamReassembler::empty() const { return unassembled_bytes() == 0 && _output.buffer_empty(); }
bool StreamReassembler::empty() const { return buffer.empty() && is_eof; }
