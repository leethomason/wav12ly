#pragma once

#include <stdint.h>
#include "./wav12/wav12stream.h"
#include "./wav12/expander.h"

int32_t* compressAndTest(const int16_t* samples, int nSamples, 
    int codec, int table,
    uint8_t* compressed, uint32_t* nCompressed, 
    int32_t* err);

class MemStream : public wav12::IStream
{
public:
    MemStream(const uint8_t* data, uint32_t dataSize);

    virtual void set(uint32_t addr, uint32_t size);
    virtual uint32_t fetch(uint8_t* buffer, uint32_t nBytes);
    virtual void rewind();
    virtual bool done() const { return m_pos == m_size; }

protected:
    const uint8_t* m_data;
    const uint8_t* m_data_end;
    uint32_t m_addr = 0;
    uint32_t m_size = 0;
    uint32_t m_pos = 0;
};

void encodeBase64(const uint8_t* bytes, int nBytes, char* target, bool writeNull);
void decodeBase64(const char* src, int nBytes, uint8_t* dst);
uint32_t hash32(const char* v, const char* end, uint32_t h = 0);




