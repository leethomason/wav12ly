#pragma once

#include <memory>
#include <stdint.h>
#include "./wav12/interface.h"
#include "./wav12/expander.h"

struct EncodedStream {
    int nSamples = 0;
    int nCompressed = 0;
    int table = 0;
    int predictor = 0;
    int32_t aveError2 = 0;
    std::unique_ptr<uint8_t[]> compressed;
    std::unique_ptr<int32_t[]> stereo;
};

EncodedStream compressS4(const int16_t* samples, int nSamples, int table, int predictor);

EncodedStream compressGroup(const int16_t* samples, int nSamples);

class MemStream : public IStream
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




