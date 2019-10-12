#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#ifdef _MSC_VER
#include <assert.h>
#define ASSERT assert
#else
#define ASSERT
#endif

using namespace wav12;

// fiati:   mse=460     -1 0 1 2 2 2 3 4
// poweron: mse=1056136 -1 0 1 2 3 3 3 4
// hum:     mse=73      -1 0 1 2 2 2 3 4

#ifndef TUNE_MODE
const 
#endif
int ExpanderAD::DELTA[TABLE_SIZE] = {
    -1, 0, 1, 2, 2, 3, 3, 4
};

void ExpanderAD::init(IStream* stream)
{
    m_stream = stream;
    m_shift = 1;
}

void ExpanderAD::rewind()
{
    m_vel = Velocity();
    m_shift = 1;
    m_stream->rewind();
}

void ExpanderAD::compress4(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed)
{
    Velocity vel;
    *nCompressed = nSamples;
    uint8_t* target = *compressed = new uint8_t[nSamples];

    int shift = 1;
    for (int i = 0; i < nSamples; ++i) {
        int value = data[i];
        int guess = vel.guess();
        int delta = value - guess;

        uint8_t sign = 0;
        if (delta < 0) {
            sign = 128;
            delta = -delta;
        }

        delta >>= shift;
        if (delta > 127) delta = 127;

        target[i] = delta | sign;

        int p = guess + (delta << shift) * (sign ? -1 : 1);
        vel.push(p);

        shift += DELTA[delta >> 4];
        if (shift < 0) shift = 0;
        if (shift > 8) shift = 8;
    }
}


int ExpanderAD::expand(int32_t *target, uint32_t nSamples, int32_t volume16, bool add)
{
    if (!m_stream)
        return 0;

    int mult = add ? 1 : 0;
    int n = 0;

    while (n < int(nSamples)) {
        int fetch = m_stream->fetch(m_buffer, wav12Min(int(nSamples) - n, BUFFER_SIZE));
        const uint8_t* p = m_buffer;

        for (int i = 0; i < fetch; ++i) {
            uint8_t delta = *p++;
            int guess = m_vel.guess();

            uint8_t sign = delta & 0x80;
            delta &= 0x7f;

            int value = guess + (delta << m_shift) * (sign ? -1 : 1);
            m_vel.push(value);

            int32_t s = value * volume16;
            target[0] = target[1] = target[0] * mult + s;
            target += 2;

            m_shift += DELTA[delta >> 4];
            if (m_shift < 0) m_shift = 0;
            if (m_shift > 8) m_shift = 8;
        }
        n += fetch;
    }
    return nSamples;
}



MemStream::MemStream(const uint8_t *data, uint32_t size)
{
    m_data = data;
    m_data_end = data + size;
    m_addr = 0;
    m_size = 0;
    m_pos = 0;
}

void MemStream::set(uint32_t addr, uint32_t size)
{
    m_addr = addr;
    m_size = size;
    m_pos = 0;
    ASSERT(m_data + m_addr + m_size <= m_data_end);
}

void MemStream::rewind()
{
    m_pos = 0;
}

uint32_t MemStream::fetch(uint8_t *buffer, uint32_t nBytes)
{
    if (m_pos + nBytes > m_size)
        nBytes = m_size - m_pos;

    memcpy(buffer, m_data + m_addr + m_pos, nBytes);
    m_pos += nBytes;
    return nBytes;
}
