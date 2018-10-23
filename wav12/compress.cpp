#include "compress.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include "bits.h"

using namespace wav12;

void wav12::compress(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed)
{
    *compressed = new uint8_t[nSamples*2];
    uint8_t* target = *compressed;

    for (int i = 0; i < nSamples; i += 2)
    {
        // Always use an even size, to avoid unaligned reads in the 16 bit section.
        int16_t v0 = data[i];
        int16_t v1 = (i + 1 < nSamples) ? data[i + 1] : 0;

        uint16_t v0u = uint16_t(v0);
        uint16_t v1u = uint16_t(v1);

        *target++ = (v0u >> 8) & 0xff;
        *target++ = (v0u & 0xf0) | ((v1u >> 12) & 0x0f);
        *target++ = (v1u >> 4) & 0xff;
    }
    size_t size = target - *compressed;
    *nCompressed = uint32_t(size);
}


Expander::Expander(uint8_t* buffer, uint32_t bufferSize)
{
    m_buffer = buffer;
    m_bufferSize = bufferSize;
    init(0, 0, 0);
}


void Expander::init(wav12::IStream* stream, uint32_t nSamples, int format)
{
    m_stream = stream;
    m_nSamples = nSamples;
    m_pos = 0;
    m_format = format;
}


void Expander::fetchSamples(int nSamples)
{
    int bytesToFetch = (nSamples * 3 + 1) / 2;
    int actual = m_stream->fetch(m_buffer, bytesToFetch);
    assert(actual == bytesToFetch);
}


/*
void Expander::expand(int16_t* target, uint32_t nSamples)
{
    m_pos += nSamples;

    if (m_format == 0) {
        int actual = m_stream->fetch((uint8_t*) target, nSamples * 2);
        assert(actual == nSamples * 2);
    }
    else {
        uint32_t i = 0;
        while (i < nSamples) {
            int nSamplesToFetch = min(nSamples - i, m_bufferSize * 5 / 8);
            fetchSamples(nSamplesToFetch);

            int16_t* t = target + i;
            uint8_t* src = m_buffer;
            for (int j = 0; j < nSamplesToFetch; j += 2) {
                // Always even samples. Pull out 2.
                EXPAND_2
            }
            i += nSamplesToFetch;
        }
    }
}
*/

void Expander::expand2(int32_t* target, uint32_t nSamples, int32_t volume)
{
    m_pos += nSamples;
    const int32_t* end = target + nSamples * 2;

    if (m_format == 0) {
        uint32_t i = 0;
        while (i < nSamples) {
            int nSamplesToFetch = min(nSamples - i, m_bufferSize / 2);
            fetchSamples(nSamplesToFetch);

            int32_t* t = target + i * 2;
            const int16_t* src = (const int16_t*) m_buffer;
            for (int j = 0; j < nSamplesToFetch; j++) {
                int32_t v = *src  * volume;
                ++src;
                *t++ = v;
                *t++ = v;
            }
            i += nSamplesToFetch;
        }
    }
    else {
        uint32_t i = 0;
        while (i < nSamples) {
            int nSamplesToFetch = min(nSamples - i, m_bufferSize / 2);
            fetchSamples(nSamplesToFetch);

            int32_t* t = target + i * 2;
            uint8_t* src = m_buffer;
            for (int j = 0; j < nSamplesToFetch; j += 2) {
                uint8_t s0 = *src++;
                uint8_t s1 = *src++;
                uint8_t s2 = *src++;
                int32_t v0 = int16_t((uint16_t(s0) << 8) | (uint16_t(s1 & 0xf0))) * volume;
                int32_t v1 = int16_t((uint16_t(s1 & 0x0f) << 12) | (uint16_t(s2) << 4)) * volume;
                *t++ = v0;
                *t++ = v0;
                if (t < end) {
                    *t++ = v1;
                    *t++ = v1;
                }
            }
            i += nSamplesToFetch;
        }
    }
}

uint32_t MemStream::fetch(uint8_t* buffer, uint32_t nBytes)
{
    uint32_t n = min(nBytes, m_size - m_pos);
    memcpy(buffer, m_data + m_pos, n);
    m_pos += n;
    return n;
}
