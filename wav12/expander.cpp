#include "expander.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

using namespace wav12;

uint8_t ExpanderAD4::m_buffer[BUFFER_SIZE] = {0};

void ExpanderAD4::init(IStream* stream)
{
    m_stream = stream;
    rewind();
}

void ExpanderAD4::rewind()
{
    m_state = S4ADPCM::State();
    m_stream->rewind();
}

void ExpanderAD4::compress4(const int16_t* data, int32_t nSamples,
    uint8_t** compressed, uint32_t* nCompressed, const int* table, int64_t* error)
{
    *compressed = new uint8_t[nSamples];

    S4ADPCM::State state;
    *nCompressed = S4ADPCM::encode4(data, nSamples, *compressed, &state, table, error);
}

void ExpanderAD4::compress8(const int16_t* data, int32_t nSamples,
    uint8_t** compressed, uint32_t* nCompressed, const int* table, int64_t* error)
{
    *nCompressed = nSamples;
    *compressed = new uint8_t[nSamples];

    S4ADPCM::State state;
    S4ADPCM::encode8(data, nSamples, *compressed, &state, table, error);
}


int ExpanderAD4::expand(int32_t *target, uint32_t nSamples, int32_t volume, bool add, 
    Codec codec, const int* table, bool overrideEasing)
{
    if (!m_stream)
        return 0;

    if (overrideEasing) {
        m_state.volumeShifted = volume << 8;
    }

    uint32_t n = 0;

    while(n < nSamples) {
        int samplesWanted = wav12Min<int>(bytesToSamples(BUFFER_SIZE, codec), nSamples - n);
        int bytesWanted = samplesToBytes(samplesWanted, codec);
        uint32_t bytesFetched = m_stream->fetch(m_buffer, bytesWanted);
        uint32_t samplesFetched = bytesToSamples(bytesFetched, codec);
        if (samplesFetched > nSamples - n)
            samplesFetched = nSamples - n;  // because 2 samples a byte. The last one can be zero.

        if (!bytesFetched)
            break;

        if (codec == Codec::BIT8)
            S4ADPCM::decode8(m_buffer, samplesFetched, volume, add, target + intptr_t(n) * 2, &m_state, table);
        else
            S4ADPCM::decode4(m_buffer, samplesFetched, volume, add, target + intptr_t(n) * 2, &m_state, table);
        n += samplesFetched;
    }
    return n;
}

