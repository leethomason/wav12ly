#ifndef WAV_COMPRESSION
#define WAV_COMPRESSION

#include "wav12stream.h"

#include <stdint.h>
#include <assert.h>
#include <string.h>

namespace wav12 {

    template<class T>
    T wav12Min(T a, T b) { return (a < b) ? a : b; }
    template<class T>
    T wav12Max(T a, T b) { return (a > b) ? a : b; }
    template<class T>
    T wav12Clamp(T x, T a, T b) {
        if (x < a) return a;
        if (x > b) return b;
        return x;
    }

    struct Wav12Header
    {
        char id[4];             // 'wv12'
        uint32_t lenInBytes;    // after header, compressed size
        uint32_t nSamples;
        uint8_t  format;        // 0 uncompressed, 1 compressed
        uint8_t  unused[3];
    };

    void compress(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed);
    bool compress8(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed, float* errorRatio);
    
    class MemStream : public wav12::IStream
    {
    public:
        MemStream(const uint8_t* data, uint32_t size);

        virtual void set(uint32_t addr, uint32_t size);
        virtual uint32_t fetch(uint8_t* buffer, uint32_t nBytes);
        virtual void rewind();

     protected:
         const uint8_t* m_data;
         uint32_t m_size;
         uint32_t m_pos;
    };


    class Expander
    {
    public:
        Expander();
        void begin(uint8_t* buffer, uint32_t bufferSize);
        void init(IStream* stream, uint32_t nSamples, int format);

        // Expand to the target buffer with a length of nTarget.
        // Returns number of samples actually expanded.
        //void expand(int16_t* target, uint32_t nTarget);

        // Does a stereo expansion (both channels the same, of course)
        // to 32 bits. nTarget is the samples per channel.
        // Volume max is 65536.
        // If 'add' is true, will add to the target buffer (for mixing), else
        // will just write & replace.
        void expand2(int32_t* target, uint32_t nTarget, int32_t volume, bool add);

        bool done() const { return m_nSamples == m_pos; }
        
        uint32_t samples() const { return m_nSamples; }
        uint32_t pos() const     { return m_pos; }
        void rewind();

    private:
        int32_t* expandComp0(int32_t* target, const int16_t* src, uint32_t n, int32_t volume, bool add);
        int32_t* expandComp1(int32_t* target, const uint8_t* src, uint32_t n, const int32_t* end, int32_t volume, bool add);
        int32_t* expandComp2(int32_t* target, const uint8_t* src, const int32_t* end, int32_t volume, bool add);


        uint32_t fetchSamples(uint32_t n);

        IStream* m_stream;
        uint32_t m_nSamples;
        uint32_t m_pos;
        int m_format;
        uint8_t* m_buffer;
        uint32_t m_bufferSize;
    };
}
#endif

