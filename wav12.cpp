#include <stdint.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <algorithm>

extern "C" {
#include "wave_reader.h"
#include "wave_writer.h"
}

#include "tinyxml2.h"
#include "memimage.h"

#include "./wav12/compress.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr, int shift);
int parseXML(const char* filename, bool textFile);

static const float M_PI = 3.14f;
static const int NSAMPLES = 63;
static const int EXPAND_BLOCK = 32;

bool checkError(const int16_t* src, const int32_t* dst, int n, int error)
{
    for (int i = 0; i < n; ++i) {
        int e0 = abs(src[i] - dst[i * 2 + 0]);
        int e1 = abs(src[i] - dst[i * 2 + 1]);

        assert(e0 < error);
        assert(e1 < error);
        if (e0 >= error) return false;
        if (e1 >= error) return false;
    }
    return true;
}


void basicTest_0()
{
    int16_t* samples = new int16_t[NSAMPLES];
    int32_t* stereo = new int32_t[NSAMPLES * 2];

    for (int i = 0; i < NSAMPLES; i++) {
        samples[i] = int16_t(1000.0f * sinf(i * 2.0f * M_PI / NSAMPLES));
    }

    uint8_t block[EXPAND_BLOCK];

    MemStream memStream((const uint8_t*) samples, NSAMPLES*2);
    Expander expander;
    expander.begin(block, EXPAND_BLOCK);
    expander.init(&memStream, NSAMPLES, 0);

    //auto start = std::chrono::high_resolution_clock::now();
    int n = 0;
    while (n < NSAMPLES) {
        int toExpand = wav12Min(NSAMPLES - n, EXPAND_BLOCK);
        expander.expand(stereo + n * 2, toExpand, 1, false);
        n += toExpand;
    }
    //auto end = std::chrono::high_resolution_clock::now();
    //std::chrono::duration<double> elapsed = end - start;

    bool okay = checkError(samples, stereo, NSAMPLES, 1);
    printf("Compress-0 check: %s\n", okay ? "okay" : "FAIL");
    //printf("Runtime: %.3f\n", elapsed.count() * 1000.0 * 1000.0);

    delete[] samples;
    delete[] stereo;
}


void basicTest_1()
{
    int16_t* samples = new int16_t[NSAMPLES];
    int32_t* stereo = new int32_t[NSAMPLES * 2];

    for (int i = 0; i < NSAMPLES; i++) {
        samples[i] = int16_t(1000.0f * sinf(i * 2.0f * M_PI / NSAMPLES));
    }

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    wav12::compress(samples, NSAMPLES, &compressed, &nCompressed);

    uint8_t block[EXPAND_BLOCK];

    MemStream memStream(compressed, nCompressed);
    Expander expander;
    expander.begin(block, EXPAND_BLOCK);
    expander.init(&memStream, NSAMPLES, 1);

    //auto start = std::chrono::high_resolution_clock::now();
    int n = 0;
    while (n < NSAMPLES) {
        int toExpand = wav12Min(NSAMPLES - n, EXPAND_BLOCK);
        expander.expand(stereo + n * 2, toExpand, 1, false);
        n += toExpand;
    }
    //auto end = std::chrono::high_resolution_clock::now();
    //std::chrono::duration<double> elapsed = end - start;

    bool okay = checkError(samples, stereo, NSAMPLES, 16);
    printf("Compress-1 check: %s\n", okay ? "okay" : "FAIL");
    //printf("Runtime: %.3f\n", elapsed.count() * 1000.0 * 1000.0);
     
    delete[] compressed;
    delete[] samples;
    delete[] stereo;
}


void basicTest_2()
{
    int16_t* samples = new int16_t[NSAMPLES];
    int32_t* stereo = new int32_t[NSAMPLES * 2];

    for (int i = 0; i < NSAMPLES; i++) {
        samples[i] = int16_t(1000.0f * sinf(i * 2.0f * M_PI / NSAMPLES));
    }

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    float errorRatio = 0;
    wav12::compress8(samples, NSAMPLES, &compressed, &nCompressed, &errorRatio);

    uint8_t block[EXPAND_BLOCK];

    MemStream memStream(compressed, nCompressed);
    Expander expander;
    expander.begin(block, EXPAND_BLOCK);
    expander.init(&memStream, NSAMPLES, 2);

    int n = 0;
    while (n < NSAMPLES) {
        int toExpand = wav12Min(NSAMPLES - n, EXPAND_BLOCK);
        expander.expand(stereo + n * 2, toExpand, 1, false);
        n += toExpand;
    }
    // Error 17 (instead of 16) is (probably) an artifact of encoding
    // in 12 bit space instead of tracking the 16 bit value. Encoder could be improved.
    bool okay = checkError(samples, stereo, NSAMPLES, 17);
    printf("Compress-2 check: %s error=%.2f%%\n", okay ? "okay" : "FAIL", errorRatio * 100.0f);

    delete[] compressed;
    delete[] samples;
    delete[] stereo;
}


void basicTest_3()
{
    int16_t* samples = new int16_t[NSAMPLES];
    int32_t* stereo = new int32_t[NSAMPLES * 2];

    for (int i = 0; i < NSAMPLES; i++) {
        samples[i] = -int16_t(1000.0f * sinf(i * 2.0f * M_PI / NSAMPLES));
    }

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    float errorRatio = 0;
    wav12::compressVelocity(samples, NSAMPLES, &compressed, &nCompressed);

    MemStream memStream(compressed, nCompressed);
    ExpanderV expander;
    expander.init(&memStream, NSAMPLES, 3);

    int n = 0;
    while (n < NSAMPLES) {
        int toExpand = wav12Min(NSAMPLES - n, EXPAND_BLOCK);
        expander.expand(stereo + n * 2, toExpand, 1, false);
        n += toExpand;
    }
    bool okay = checkError(samples, stereo, NSAMPLES, 16);
    printf("Compress-3 check: %s\n", okay ? "okay" : "FAIL");

    delete[] compressed;
    delete[] samples;
    delete[] stereo;
}


void basicTest_3b()
{
    static const int NS = 513;
    int16_t* samples = new int16_t[NS];
    int32_t* stereo = new int32_t[NS * 2];

    for (int i = 0; i < NS; i++) {
        samples[i] = -int16_t((100.f + i*10.f) * sinf(i * 2.0f * M_PI / NS));
    }

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    float errorRatio = 0;
    wav12::compressVelocity(samples, NS, &compressed, &nCompressed);

    MemStream memStream(compressed, nCompressed);
    ExpanderV expander;
    expander.init(&memStream, NS, 3);

    int n = 0;
    while (n < NS) {
        int toExpand = wav12Min(NS - n, EXPAND_BLOCK);
        expander.expand(stereo + n * 2, toExpand, 1, false);
        n += toExpand;
    }
    bool okay = checkError(samples, stereo, NS, 16);
    printf("Compress-3b check: %s\n", okay ? "okay" : "FAIL");

    delete[] compressed;
    delete[] samples;
    delete[] stereo;
}


int main(int argc, const char* argv[])
{
    basicTest_0();
    basicTest_1();
    basicTest_2();
    basicTest_3();
    basicTest_3b();

    if (argc < 2) {
        printf("Usage:\n");
        printf("    wav12 filename                         Runs tests on 'filename'\n");
        printf("    wav12 xmlFile                          Creates memory image.\n");
        printf("Options:\n");
        printf("    -t, write text file.\n");
        printf("    -2, use compress8.\n");
        return 1;
    }
    if (strstr(argv[1], ".xml")) {
        bool textFile = false;
        if (argc == 3 && strcmp(argv[2], "-t") == 0) {
            textFile = true;
        }
        int rc = parseXML(argv[1], textFile);
        printf("MemImage generation return code=%d\n", rc);
        return rc;
    }

    wave_reader_error error;
    wave_reader* wr = wave_reader_open(argv[1], &error);

    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    printf("Format=%d channels=%d rate=%d\n", format, nChannels, rate);

    if (format != 1
        || nChannels != 1
        || rate != 22050)
    {
        printf("Input must be 22050 Hz Mono\n");
        return 1;
    }

    printf("Running basic tests (comp=2) on '%s'\n", argv[1]);
    runTest(wr, 2);
    wave_reader_close(wr);
    return 0;
}


bool runTest(wave_reader* wr, int compress)
{
    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);
    int error = compress ? 1 << 4 : 1;

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

    uint8_t* compressed = 0;
    uint32_t nCompressed = 0;
    float errorRatio = 0;

    if (compress == 2) {
        bool okay = wav12::compress8(data, nSamples, &compressed, &nCompressed, &errorRatio);
        if (okay) {
            printf("Error=%.2f%%\n", errorRatio * 100.f);
        }
        else {
            printf("Can't compress-2\n");
            compress = 1;
        }
    }

    assert(compress);
    if (compress == 1) {
        wav12::compress(data, nSamples, &compressed, &nCompressed);
    }

    static const int BUFSIZE = 256;
    uint8_t buffer[BUFSIZE];

    int32_t* stereoData = new int32_t[nSamples*2];
    MemStream memStream(compressed, nCompressed);
    Expander expander;
    expander.begin(buffer, BUFSIZE);
    expander.init(&memStream, nSamples, compress);

    static const int STEP = 1024;
    for (int i = 0; i < nSamples; i += STEP) {
        int n = std::min(STEP, nSamples - i);
        expander.expand(stereoData + i*2, n, 1, false);
    }

    bool okay = true;
    if (compress == 1) {
        for (int i = 0; i < nSamples; i++) {
            if (abs(data[i] - stereoData[i * 2]) >= error) {
                assert(false);
                okay = false;
            }
        }
        printf("Test full stream: %s\n", okay ? "PASS" : "FAIL");
    }
    else if (compress == 2) {
        uint64_t error2 = 0;
        for (int i = 0; i < nSamples; i++) {
            int e = (data[i] / 16) - (stereoData[i * 2] / 16);
            error2 += e * e;
        }
        printf("Ave error^2 (12 bit): %.1f\n", double(error2) / double(nSamples));
    }

    int16_t* monoData = new int16_t[nSamples];
    for (int i = 0; i < nSamples; ++i) {
        monoData[i] = stereoData[i * 2];
    }

    {
        wave_writer_format format = { 1, 22050, 16 };
        wave_writer_error error = WW_NO_ERROR;
        wave_writer* ww = wave_writer_open("test.wav", &format, &error);
        wave_writer_put_samples(ww, nSamples, monoData);
        wave_writer_close(ww, &error);
    }
    delete[] monoData;
    delete[] stereoData;
    return okay;
}


std::string stdString(const char* p)
{
    std::string s;
    while (*p) {
        s += tolower(*p);
        p++;
    }
    if (s.size() > MemUnit::NAME_LEN) {
        s.resize(MemUnit::NAME_LEN, ' ');
    }
    return s;
}


int parseXML(const char* filename, bool textFile)
{
    XMLDocument doc;
    doc.LoadFile(filename);
    if (doc.Error()) return 1;

    MemImageUtil image;

    for (const XMLElement* dirElement = doc.RootElement()->FirstChildElement();
        dirElement;
        dirElement = dirElement->NextSiblingElement()) 
    {
        std::string stdDirName = stdString(dirElement->Attribute("path"));
        image.addDir(stdDirName.c_str());

        for (const XMLElement* fileElement = dirElement->FirstChildElement();
            fileElement;
            fileElement = fileElement->NextSiblingElement())
        {
            const char* fname = fileElement->Attribute("path");
            const char* extension = strrchr(fname, '.');
            std::string stdfname;
            stdfname.append(fname, extension);

            std::string fullPath = stdDirName;
            fullPath += '/';
            fullPath += fname;

            wave_reader_error error = WR_NO_ERROR;
            wave_reader* wr = wave_reader_open(fullPath.c_str(), &error);
            if (error != WR_NO_ERROR) return error;

            int format = wave_reader_get_format(wr);
            int nChannels = wave_reader_get_num_channels(wr);
            int rate = wave_reader_get_sample_rate(wr);
            int nSamples = wave_reader_get_num_samples(wr);

            if (format != 1 || nChannels != 1 || rate != 22050) {
                printf("Input '%s' must be 22050 Hz Mono, freq=%d channels=%d\n", fname, rate, nChannels);
                return 100;
            }

            int16_t* data = new int16_t[nSamples];
            wave_reader_get_samples(wr, nSamples, data);
            uint8_t* compressed = 0;
            uint32_t nCompressed = 0;
      
            float errorRatio = 0;
            int desiredCompression = fileElement->IntAttribute("compress", 2);
            int compressionFormat = 2;

            wav12::compressVelocity(data, nSamples, &compressed, &nCompressed);
            printf("Compressed velocity=%d\n", 100 * nCompressed / (nSamples*2));
            compressionFormat = 3;
      
#if 0
            delete[] compressed;

            if (   desiredCompression == 2 
                && wav12::compress8(data, nSamples, &compressed, &nCompressed, &errorRatio)
                && errorRatio < 0.2f)
            {
                compressionFormat = 2;
            }
            else {
                delete[] compressed;
                if (desiredCompression) {
                    compressionFormat = 1;
                    wav12::compress(data, nSamples, &compressed, &nCompressed);
                }
                else {
                    compressionFormat = 0;
                }
            }
#endif
            printf("%s desired=%d format=%d error (in 2)=%.3f comp=%d\n", 
                fname, desiredCompression, compressionFormat, errorRatio,
                100 * nCompressed / (nSamples *2));
            Wav12Header header;
            memset(&header, 0, sizeof(header));
            header.id[0] = 'w';
            header.id[1] = 'l';
            header.id[2] = '1';
            header.id[3] = '2';
            header.lenInBytes = compressionFormat ? nCompressed : nSamples * 2;
            header.nSamples = nSamples;
            header.format = compressionFormat;

            uint8_t* mem = new uint8_t[sizeof(Wav12Header) + header.lenInBytes];
            memcpy(mem, &header, sizeof(Wav12Header));
            if (compressionFormat)
                memcpy(mem + sizeof(Wav12Header), compressed, nCompressed);
            else
                memcpy(mem + sizeof(Wav12Header), data, nSamples*2);

            image.addFile(stdfname.c_str(), mem, header.lenInBytes + sizeof(Wav12Header));

            delete[] mem;
            delete[] data;
            delete[] compressed;
            wave_reader_close(wr);
        }
    }

    image.dumpConsole();
    image.write("memimage.bin");
    if (textFile)
        image.writeText("memimage.txt");
    return 0;
}