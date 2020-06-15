/*
MIT License

Copyright (c) 2018 Meng Rao <raomeng1@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once
#include "endian.h"

namespace tcpshm {

struct MsgHeader
{
    // size of this msg, including header itself
    // auto set by lib, can be read by user
    uint32_t size;
    // msg type of app msg is set by user and must not be 0
    uint32_t msg_type;
    // internally used for ptcp, must not be modified by user
    uint32_t ack_seq;
    // send index
    uint32_t index;

    template<bool ToLittle>
    void ConvertByteOrder() {
        Endian<ToLittle> ed;
        ed.ConvertInPlace(size);
        ed.ConvertInPlace(msg_type);
        ed.ConvertInPlace(ack_seq);
        ed.ConvertInPlace(index);
    }
};

static const int BLK_SIZE = sizeof(MsgHeader);

static const inline int BlockAlign(int size) {
    int tmp = -1 * BLK_SIZE;
    return ((size + BLK_SIZE - 1) & tmp);
}

static const inline int BlockCount(int size) {
    return ((size + BLK_SIZE - 1) / BLK_SIZE);
}


} // namespace tcpshm

