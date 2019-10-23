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
#include "wavutil.h"

#include "./wav12/compress.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr);
int parseXML(const char* filename, bool textFile);

static const float M_PI = 3.14f;
static const int NSAMPLES = 63;
static const int EXPAND_BLOCK = 32;

void basicTest_4()
{
    static const int NS = 23;
    int32_t* stereo = new int32_t[NS * 2];

    int16_t samples[NS] = {
        100, 200, 400, 800, 1600, 2000, 2000, 1800, 
        1500, 1200, 800, 400, 0, -400, -800, -1000,
        -900, -700, -500, -400, -300, -200, -100
    };

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    ExpanderAD4::compress4(samples, NS, &compressed, &nCompressed);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 8;
    for (int i = 0; i < NS; i += STEP) {
        expander.expand(stereo+i*2, wav12Min(STEP, NS - i), 256, false, false);
    }
    // Convert from 32 bit output at back to 16 bit output.
    for (int i = 0; i < NS; ++i) {
        stereo[i * 2 + 0] >>= 16;
        stereo[i * 2 + 1] >>= 16;
    }
    
    printf("Compress 4\n");
    for (int i = 0; i < NS; ++i) {
        printf("%4d ", samples[i]);
    }
    printf("\n");
    for (int i = 0; i < NS; ++i) {
        printf("%4d ", stereo[i*2]);
    }
    printf("\n");

    delete[] compressed;
    delete[] stereo;
}


void basicTest_8()
{
    static const int NS = 23;
    int32_t* stereo = new int32_t[NS * 2];

    int16_t samples[NS] = {
        100, 200, 400, 800, 1600, 2000, 2000, 1800,
        1500, 1200, 800, 400, 0, -400, -800, -1000,
        -900, -700, -500, -400, -300, -200, -100
    };

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    ExpanderAD4::compress8(samples, NS, &compressed, &nCompressed);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 8;
    for (int i = 0; i < NS; i += STEP) {
        expander.expand(stereo + i * 2, wav12Min(STEP, NS - i), 256, false, true);
    }
    // Convert from 32 bit output at back to 16 bit output.
    for (int i = 0; i < NS; ++i) {
        stereo[i * 2 + 0] >>= 16;
        stereo[i * 2 + 1] >>= 16;
    }

    printf("Compress 8\n");
    for (int i = 0; i < NS; ++i) {
        printf("%4d ", samples[i]);
    }
    printf("\n");
    for (int i = 0; i < NS; ++i) {
        printf("%4d ", stereo[i * 2]);
    }
    printf("\n");

    delete[] compressed;
    delete[] stereo;
}

int main(int argc, const char* argv[])
{
    basicTest_4();
    basicTest_8();

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

    printf("Running basic tests on '%s'\n", argv[1]);
    runTest(wr);
    wave_reader_close(wr);
    return 0;
}


int32_t* testCompress(const int16_t* data, int nSamples, int* _mse, bool use8Bit)
{
    uint8_t* compressed = 0;
    uint32_t nCompressed = 0;

    if (use8Bit)
        ExpanderAD4::compress8(data, nSamples, &compressed, &nCompressed);
    else
        ExpanderAD4::compress4(data, nSamples, &compressed, &nCompressed);

    int32_t* stereoData = new int32_t[nSamples * 2];
    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 1024;
    static const int VOLUME = 256;

    for (int i = 0; i < nSamples; i += STEP) {
        int n = std::min(STEP, nSamples - i);
        expander.expand(stereoData + i * 2, n, VOLUME, false, use8Bit);
    }

    int64_t err = 0;
    for (int i = 0; i < nSamples; ++i) {
        int64_t e = (data[i] - (stereoData[i * 2] >> 16));
        err += e * e;
        assert(err >= 0);
    }
    int64_t mse = err / nSamples;
    if (_mse) *_mse = (int)mse;

    delete[] compressed;
    return stereoData;
}


void saveOut(const char* fname, const int32_t* stereo, int nSamples)
{
    int16_t* s16 = new int16_t[nSamples];
    for (int i = 0; i < nSamples; ++i) {
        s16[i] = stereo[i * 2] / 65536;
    }

    wave_writer_format writeFormat = { 1, 22050, 16 };
    wave_writer_error error = WW_NO_ERROR;
    wave_writer* ww = wave_writer_open(fname, &writeFormat, &error);
    wave_writer_put_samples(ww, nSamples, (void*) s16);
    wave_writer_close(ww, &error);

    delete[] s16;
}

bool runTest(wave_reader* wr)
{
    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

#ifdef TUNE_MODE
    int bestMSE = LONG_MAX;
    for (int bits = 0; bits < 256; ++bits) {
        int val = -2;
        for (int i = 0; i < 8; ++i) {
            if (bits & (1 << i)) val += 1;
            EXPANDER::DELTA[i] = val;
        }
        if (val >= 2) {
            int mse = 0;
            int32_t* stereo = testCompress4(data, nSamples, &mse);
            delete[] stereo;
            if (mse < bestMSE) {
                printf("mse=%d ", mse);
                for (int i = 0; i < 8; ++i)
                    printf("%d ", EXPANDER::DELTA[i]);
                printf("\n");
                bestMSE = mse;
            }
        }
    }

#else
    int mse = 0;
    int32_t* stereoData = testCompress(data, nSamples, &mse, false);
    saveOut("test4.wav", stereoData, nSamples);
    delete[] stereoData;
    printf("4 bit mse=%d\n", mse);

    stereoData = testCompress(data, nSamples, &mse, true);
    saveOut("test8.wav", stereoData, nSamples);
    delete[] stereoData;
    printf("8 bit mse=%d\n", mse);

#endif
    return true;
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

            bool use8Bit = false;
            if (fileElement->IntAttribute("compression") == 8) {
                use8Bit = true;
            }

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
            int32_t stages[3] = { 0 };

            uint8_t* compressed = 0;
            uint32_t nCompressed = 0;
            int mse = 0;
            int32_t* stereo = compressAndTest(data, nSamples, use8Bit, &compressed, &nCompressed, &mse);
            image.addFile(stdfname.c_str(), compressed, nCompressed, nSamples, use8Bit, mse);

            delete[] compressed;
            delete[] data;
            delete[] stereo;
            wave_reader_close(wr);
        }
    }

    image.dumpConsole();
    image.write("memimage.bin");
    if (textFile)
        image.writeText("memimage.txt");
    return 0;
}