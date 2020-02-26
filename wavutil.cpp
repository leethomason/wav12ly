#include "wavutil.h"
#include "./wav12/expander.h"

#include <assert.h>
#include <string.h>

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
        wav12::ExpanderAD4::compress8(samples, nSamples, compressed, nCompressed);
    else
        wav12::ExpanderAD4::compress4(samples, nSamples, compressed, nCompressed);

    int32_t* stereoData = new int32_t[nSamples * 2];
    MemStream memStream(*compressed, *nCompressed);
    memStream.set(0, *nCompressed);
    wav12::ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 1024;
    static const int VOLUME = 256;

    for (int i = 0; i < nSamples; i += STEP) {
        int n = wav12::wav12Min(STEP, nSamples - i);
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

char base64BitsToChar(int b)
{
    if (b >= 0 && b < 26)
        return 'A' + b;
    if (b >= 26 && b < 52)
        return 'a' + (b - 26);
    if (b >= 52 && b < 62)
        return '0' + (b - 52);
    if (b == 62)
        return '+';
    if (b == 63)
        return '-';
    return 0;
}

int base64CharToBits(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    if (c == '+')
        return 62;
    if (c == '-')
        return 63;
    return 0;
}

void encodeBase64(const uint8_t* src, int nBytes, char* dst, bool writeNull)
{
    // base64 - 6 bits per char of output
    // every 3 bytes (24 bits) is 4 char

    char* t = dst;
    for (int i = 0; i < nBytes; i += 3) {
        uint32_t accum = 0;
        accum = src[i];
        if (i + 1 < nBytes)
            accum |= src[i + 1] << 8;
        if (i + 2 < nBytes)
            accum |= src[i + 2] << 16;

        *t++ = base64BitsToChar(accum & 63);
        *t++ = base64BitsToChar((accum >> 6) & 63);
        *t++ = base64BitsToChar((accum >> 12) & 63);
        *t++ = base64BitsToChar((accum >> 18) & 63);
    }
    if (writeNull)
        *t = 0;
}

void decodeBase64(const char* src, int nBytes, uint8_t* dst)
{
    const char* p = src;
    for (int i = 0; i < nBytes; i += 3) {
        uint32_t accum = 0;
        accum = base64CharToBits(*p++);
        accum |= base64CharToBits(*p++) << 6;
        accum |= base64CharToBits(*p++) << 12;
        accum |= base64CharToBits(*p++) << 18;

        dst[i] = accum & 0xff;
        if (i + 1 < nBytes) dst[i + 1] = (accum >> 8) & 0xff;
        if (i + 2 < nBytes) dst[i + 2] = (accum >> 16) & 0xff;
    }
}


uint32_t hash32(const char* v, const char* end, uint32_t h)
{
    for (; v < end; ++v) {
        h = ((h << 5) + h) ^ (*v);
    }
    return h;
}
