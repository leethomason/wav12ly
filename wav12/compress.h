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

    struct Velocity
    {
        int prev2 = 0;
        int prev1 = 0;
        int guess() const { return 2 * prev1 - prev2; }
        void push(int value) {
            prev2 = prev1;
            prev1 = value;
        }
    };

    // Codec 0 is uncompressed.
    // Codec 1 is 12 bit (loss)
    // Codec 2 is 12 bit (loss) with delta in a frame (63%)
    // Codec 3 is 12 bit (loss) predictive, and already better (58%)
    // Codec 3b is 12 bit (loss) predictive, uses extra bits, and gets to 55%
    void compress(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed);
    bool compress8(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed, float* errorRatio);
    bool compressVelocity(const int16_t* data, int32_t nSamples, uint8_t** compressed, uint32_t* nCompressed);

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

    class ExpanderV
    {
    public:
        static const int BUFFER_SIZE = 256;

        ExpanderV() {}
        void init(IStream* stream, uint32_t nSamples, int format);

        void expand(int32_t* target, uint32_t nTarget, int32_t volume, bool add);
        bool done() const { return m_done; }
        void rewind();

    private:
        inline bool hasSample() {
            if (m_bufferStart < m_bufferEnd - 1)
                return true;

            if (m_bufferStart == m_bufferEnd - 1 &&
                m_buffer[m_bufferStart] & 0x80)
                return true;

            return false;
        }
        void fetch();

        uint8_t m_buffer[BUFFER_SIZE];
        IStream* m_stream = 0;
        int m_bufferEnd = 0;      // position in the buffer
        int m_bufferStart = 0;
        bool m_done = false;

        // State for decompression
        Velocity m_vel;
        uint32_t m_high3 = 0;
        bool m_hasHigh3 = false;

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
        void expand(int32_t* target, uint32_t nTarget, int32_t volume, bool add);

        bool done() const { return m_nSamples == m_pos; }
        
        uint32_t samples() const { return m_nSamples; }
        uint32_t pos() const     { return m_pos; }
        void rewind();

    private:
        int32_t* expandComp0(int32_t* target, const int16_t* src, uint32_t n, int32_t volume, bool add);
        int32_t* expandComp1(int32_t* target, const uint8_t* src, uint32_t n, const int32_t* end, int32_t volume, bool add);
        int32_t* expandComp2(int32_t* target, const uint8_t* src, const int32_t* end, int32_t volume, bool add);

        IStream* m_stream;
        uint32_t m_nSamples;
        uint32_t m_pos;
        int m_format;
        uint8_t* m_buffer;
        uint32_t m_bufferSize;
    };
}
#endif

