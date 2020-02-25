#include "wavutil.h"
#include "./wav12/expander.h"

#include <assert.h>
#include <string.h>

using namespace wav12;

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
    assert(m_data + m_addr + m_size <= m_data_end);
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


int32_t* compressAndTest(const int16_t* samples, int nSamples,
    bool use8Bit,
    uint8_t** compressed, uint32_t* nCompressed,
    int* _mse)
{
    if (use8Bit)    
        ExpanderAD4::compress8(samples, nSamples, compressed, nCompressed);
    else
        ExpanderAD4::compress4(samples, nSamples, compressed, nCompressed);

    int32_t* stereoData = new int32_t[nSamples * 2];
    MemStream memStream(*compressed, *nCompressed);
    memStream.set(0, *nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 1024;
    static const int VOLUME = 256;

    for (int i = 0; i < nSamples; i += STEP) {
        int n = wav12Min(STEP, nSamples - i);
        expander.expand(stereoData + i * 2, n, VOLUME, false, use8Bit, true);
    }

    int64_t err = 0;
    for (int i = 0; i < nSamples; ++i) {
        int64_t e = (samples[i] - (stereoData[i * 2] >> 16));
        err += e * e;
        assert(err >= 0);
    }
    int64_t mse = err / nSamples;
    if (_mse) *_mse = (int)mse;

    return stereoData;
}

