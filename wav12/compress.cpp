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

bool wav12::compressVelocity(const int16_t *data, int32_t nSamples, uint8_t **compressed, 
    uint32_t *nCompressed, int32_t* stages)
{
    Velocity vel;

    *compressed = new uint8_t[nSamples * 2];
    uint8_t *target = *compressed;

    static const int MAX_STACK = 16;
    uint8_t* stack[ExpanderV::MAX_STACK]; // Pointer to locations of where to write high bits.
    int nStack = 0;

    for (int i = 0; i < nSamples; i++)
    {
        int value = data[i] / 16;
        int guess = vel.guess();
        int delta = value - guess;

        if (nStack && delta < 512 && delta >= -512)
        {
            static const int BIAS = 512;
            uint32_t bits = delta + BIAS;
            uint8_t low7 = (bits & 0x7f);
            uint8_t high3 = (bits & 0x380) >> 7;
            ASSERT((high3 << 7 | low7) == bits);
            *target++ = low7 | 0x80;

            nStack--;
            ASSERT(((*stack[nStack]) & 0xe0) == 0);
            *stack[nStack] |= (high3 << 5);

            if (stages) stages[1]++;
        }
        else if (delta < 64 && delta >= -64)
        {
            ASSERT(!nStack);    // Since we always flush the stack when possible,
                                // there shouldn't be a way to get to this if stack has data.
            static const int BIAS = 64;
            uint8_t bits = delta + BIAS;
            *target++ = bits | 0x80;
            if (stages) stages[0]++;
        }
        else
        {
            ASSERT(value < 2048 && value >= -2048);
            static const int BIAS = 2048;
            int32_t bits = BIAS + value;
            uint32_t low7 = bits & 0x07f;
            uint32_t high5 = (bits & 0xf80) >> 7;

            *target++ = low7;
            *target = high5;

            if (nStack < ExpanderV::MAX_STACK) {
                stack[nStack] = target;
                nStack++;
            }
            target++;

            if (stages) stages[2]++;
        }
        vel.push(value);
    }
    size_t size = target - *compressed;
    *nCompressed = uint32_t(size);

    return false;
}

void ExpanderV::init(IStream *stream)
{
    m_stream = stream;
}

void ExpanderV::rewind()
{
    m_bufferEnd = m_bufferStart = 0;
    m_vel = Velocity();
    m_nStack = 0;
    m_stream->rewind();
}

void ExpanderV::fetch()
{
    // Each sample is one or two bytes.
    uint32_t read = 0;
    if (m_bufferStart < m_bufferEnd)
    {
        // In this case, we have the 1st byte of a pair, but
        // not the 2nd. So the 2nd has to be there, and
        // we can't be done.
        ASSERT(m_bufferStart == m_bufferEnd - 1);
        ASSERT(m_bufferStart > 0);
        m_buffer[0] = m_buffer[m_bufferStart];
        read = m_stream->fetch(m_buffer + 1, BUFFER_SIZE - 1);
        ASSERT(read > 0);
        m_bufferEnd = read + 1;
    }
    else {
        // We were on a sample boundary, so read as much as possible.
        read = m_stream->fetch(m_buffer, BUFFER_SIZE);
        m_bufferEnd = read;
    }
    m_bufferStart = 0;
}

int ExpanderV::expand(int32_t *target, uint32_t nSamples, int32_t volume, bool add)
{
    if (!m_stream)
        return 0;

    int mult = add ? 1 : 0;

    for (uint32_t i = 0; i < nSamples; ++i)
    {
        if (m_bufferEnd == m_bufferStart ||
            ((m_bufferStart + 1 == m_bufferEnd) && (m_buffer[m_bufferStart] & 0x80) == 0))
        {
            fetch();
            if (m_bufferEnd == 0)
                return i;
        }

        const uint8_t *src = m_buffer + m_bufferStart;
        const int32_t guess = m_vel.guess();
        int32_t value = 0;

        // If the high bit is a set, it is a 1 byte sample.
        // If extra bits are on the stack, they are appiled to increase the range.
        if (src[0] & 0x80)
        {
            int32_t low7 = src[0] & 0x7f;

            if (m_nStack)
            {
                --m_nStack;
                int32_t high3 = m_stack[m_nStack];
                ASSERT(high3 >= 0 && high3 < 8);

                static const int32_t BIAS = 512;
                int32_t bits = ((high3 << 7) | low7);
                int32_t delta = bits - BIAS;
                value = guess + delta;
            }
            else
            {
                static const int32_t BIAS = 64;
                int32_t delta = low7 - BIAS;
                value = guess + delta;
            }
            m_bufferStart++;
        }
        else
        {
            // Two byte sample, since the high bit is NOT set.
            // 1 bit (clear) flag, 3 bits of storage for the next sample,
            // and 12 bits of value.
            //
            // Stored as: low7 high5
            static const int32_t BIAS = 2048;
            if (m_nStack < MAX_STACK) {
                m_stack[m_nStack++] = (src[1] & 0xe0) >> 5;
            }

            int32_t low7 = src[0] & 0x7f;
            int32_t high5 = (src[1] & 0x1f);
            int32_t bits = low7 | (high5 << 7);
            value = bits - BIAS;

            m_bufferStart += 2;
        }
        m_vel.push(value);
        int32_t s = value * volume * 16;

        target[0] = target[1] = target[0] * mult + s;
        target += 2;
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
