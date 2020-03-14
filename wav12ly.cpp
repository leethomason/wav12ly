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

#include "./wav12/expander.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr, int compressBits);
int parseXML(const char* filename, const std::string& inputPath, bool textFile);

static const float M_PI = 3.14f;
static const int NSAMPLES = 63;
static const int EXPAND_BLOCK = 32;

void test_1()
{
    static const int NS = 23;
    static const int OUT = 200;
    int32_t* stereo = new int32_t[OUT * 2];
    memset(stereo, 0, OUT * 2 * sizeof(int32_t));

    int16_t samples[NS] = {
        100, 200, 400, 800, 1600, 2000, 2000, 1800,
        1500, 1200, 800, 400, 0, -400, -800, -1000,
        -900, -700, -500, -400, -300, -200, -100
    };

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    int64_t error = 0;
    ExpanderAD4::compress4(samples, NS, &compressed, &nCompressed, &error);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    int decoded = expander.expand(stereo, OUT, 256, false, ExpanderAD4::Codec::BIT4, true);
    assert(decoded == NS + 1);  // rounds up even

    for (int i = 0; i < NS; ++i) {
        assert(stereo[i * 2] != 0);
    }
    for (int i = NS + 1; i < OUT; ++i) {
        assert(stereo[i * 2] == 0);
    }
}

void test_2()
{
    static const int NS = 23;
    static const int OUT = 200;
    int32_t* stereo = new int32_t[OUT * 2];
    memset(stereo, 0, OUT * 2 * sizeof(int32_t));

    int16_t samples[NS] = {
        100, 200, 400, 800, 1600, 2000, 2000, 1800,
        1500, 1200, 800, 400, 0, -400, -800, -1000,
        -900, -700, -500, -400, -300, -200, -100
    };

    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    int64_t error = 0;
    ExpanderAD4::compress4(samples, NS, &compressed, &nCompressed, &error);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    for (int i = 0; i < OUT; i += 10) {
        int decoded = expander.expand(stereo + i * 2, 10, 256, false, ExpanderAD4::Codec::BIT4, true);
        if (decoded == 0) break;
    }
    for (int i = 0; i < NS; ++i) {
        assert(stereo[i * 2] != 0);
    }
    for (int i = NS + 1; i < OUT; ++i) {
        assert(stereo[i * 2] == 0);
    }
}


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
    int64_t error = 0;
    ExpanderAD4::compress4(samples, NS, &compressed, &nCompressed, &error);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 8;
    for (int i = 0; i < NS; i += STEP) {
        expander.expand(stereo + i * 2, wav12Min(STEP, NS - i), 256, false, ExpanderAD4::Codec::BIT4, true);
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
        printf("%4d ", stereo[i * 2]);
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
    int64_t error = 0;
    ExpanderAD4::compress8(samples, NS, &compressed, &nCompressed, &error);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 8;
    for (int i = 0; i < NS; /* none */) {
        i += expander.expand(stereo + i * 2, STEP, 256, false, ExpanderAD4::Codec::BIT8, true);
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

void basicTest_8Add()
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
    int64_t error = 0;
    ExpanderAD4::compress8(samples, NS, &compressed, &nCompressed, &error);

    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    for (int i = 0; i < NS; /* none */) {
        i += expander.expand(stereo + i * 2, 6, 256, false, ExpanderAD4::Codec::BIT8, true);
    }
    expander.rewind();
    for (int i = 0; i < NS; /* none */) {
        i += expander.expand(stereo + i * 2, 10, 256, true, ExpanderAD4::Codec::BIT8, true);
    }

    // Convert from 32 bit output at back to 16 bit output.
    for (int i = 0; i < NS; ++i) {
        stereo[i * 2 + 0] >>= 16;
        stereo[i * 2 + 1] >>= 16;
    }

    printf("Compress 8Add\n");
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


int16_t* covert44to22(int nSamples, int16_t* data, int* nSamplesOut)
{
    int n22 = nSamples / 2;
    int16_t* s16 = new int16_t[n22];

    for (int i = 0; i < n22; ++i) {
        int16_t s = (data[i * 2] + data[i * 2 + 1]) / 2;
        s16[i] = s;
    }
    *nSamplesOut = n22;
    return s16;
}

int main(int argc, const char* argv[])
{
    test_1();
    test_2();
    basicTest_4();
    basicTest_8();

    if (argc < 2) {
        printf("Usage:\n");
        printf("    wav12 filename                Runs tests on 'filename'\n");
        printf("    wav12 xmlFile <options>       Creates memory image.\n");
        printf("Options:\n");
        printf("    -t, write text file.\n");
        printf("    -i, base input path for file leading.\n");
        return 1;
    }

    bool writeText = false;
    std::string inputPath = "";

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "-t") == 0) {
            writeText = true;
        }
        if (strcmp(argv[i], "-i") == 0) {
            inputPath = argv[i + 1];
        }
    }

    if (strstr(argv[1], ".xml")) {
        int rc = parseXML(argv[1], inputPath, writeText);
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
    runTest(wr, 4);
    wave_reader_close(wr);
    return 0;
}


int32_t* testCompress(const int16_t* data, int nSamples, int64_t* e12, ExpanderAD4::Codec codec, bool overrideEasing)
{
    uint8_t* compressed = 0;
    uint32_t nCompressed = 0;
    ExpanderAD4::compress(codec, data, nSamples, &compressed, &nCompressed, e12);

    int32_t* stereoData = new int32_t[nSamples * 2];
    MemStream memStream(compressed, nCompressed);
    memStream.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream);

    static const int STEP = 1024;
    static const int VOLUME = 256;

    for (int i = 0; i < nSamples; i += STEP) {
        // The stream has a cap; so it can fetch as many as possible each time.
        expander.expand(stereoData + i * 2, wav12Min(STEP, nSamples - i), VOLUME, false, codec, overrideEasing);
    }

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
    wave_writer_put_samples(ww, nSamples, (void*)s16);
    wave_writer_close(ww, &error);

    delete[] s16;
}

bool runTest(wave_reader* wr, int compressBits)
{
    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

#ifdef S4ADPCM_OPT
    int64_t bestE = INT64_MAX;
    printf("compression bits=%d\n", compressBits);

    for (int bits = 0; bits < 256; ++bits) {
        int val = -2;
        for (int i = 0; i < S4ADPCM::TABLE_SIZE; ++i) {
            if (bits & (1 << i)) val += 1;
            if (compressBits == 8)
                S4ADPCM::DELTA_TABLE_8[i] = val;
            else
                S4ADPCM::DELTA_TABLE_4[i] = val;
        }
        if (val >= 2) {
            int64_t err = 0;
            int32_t* stereo = testCompress(data, nSamples, &err, compressBits == 8 ? ExpanderAD4::Codec::BIT8 : ExpanderAD4::Codec::BIT4, true);
            delete[] stereo;
            if (err < bestE) {
                printf("e12(k)=%d ", int(err / 1000));
                for (int i = 0; i < S4ADPCM::TABLE_SIZE; ++i)
                    if (compressBits == 8)
                        printf("%d ", S4ADPCM::DELTA_TABLE_8[i]);
                    else
                        printf("%d ", S4ADPCM::DELTA_TABLE_4[i]);

                printf("\n");
                bestE = err;
            }
        }
    }
#else
    int64_t mse = 0;

    int32_t* stereoData = testCompress(data, nSamples, &mse, ExpanderAD4::Codec::BIT4, true);
    saveOut("test4.wav", stereoData, nSamples);
    delete[] stereoData;
    printf("4  bit mse=%lld\n", mse);

    stereoData = testCompress(data, nSamples, &mse, ExpanderAD4::Codec::BIT4, false);
    saveOut("test4Eased.wav", stereoData, nSamples);
    delete[] stereoData;

    stereoData = testCompress(data, nSamples, &mse, ExpanderAD4::Codec::BIT8, true);
    saveOut("test8.wav", stereoData, nSamples);
    delete[] stereoData;
    printf("8  bit mse=%lld\n", mse);

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


int parseXML(const char* filename, const std::string& inputPath, bool textFile)
{
    XMLDocument doc;
    doc.LoadFile(filename);

    if (doc.Error()) {
        printf("XML error: %s\n", doc.ErrorName());
        return 1;
    }

    MemImageUtil image;

    for (const XMLElement* dirElement = doc.RootElement()->FirstChildElement();
        dirElement;
        dirElement = dirElement->NextSiblingElement())
    {
        const char* p = dirElement->Attribute("path");
        std::string stdDirName = p;
        std::string fontName = stdDirName;
        if (dirElement->Attribute("name")) {
            fontName = dirElement->Attribute("name");
        }
        image.addDir(fontName.c_str());

        for (const XMLElement* fileElement = dirElement->FirstChildElement();
            fileElement;
            fileElement = fileElement->NextSiblingElement())
        {
            const char* fname = fileElement->Attribute("path");
            const char* extension = strrchr(fname, '.');
            std::string stdfname;
            stdfname.append(fname, extension);

            std::string fullPath = inputPath;
            fullPath += stdDirName;
            fullPath += '/';
            fullPath += fname;

            wav12::ExpanderAD4::Codec codec = wav12::ExpanderAD4::Codec::BIT4;
            if (fileElement->IntAttribute("compression") == 8) {
                codec = wav12::ExpanderAD4::Codec::BIT8;
            }

            wave_reader_error error = WR_NO_ERROR;
            wave_reader* wr = wave_reader_open(fullPath.c_str(), &error);
            if (error != WR_NO_ERROR) {
                printf("Failed to open: %s\n", fullPath.c_str());
                return error;
            }

            int format = wave_reader_get_format(wr);
            int nChannels = wave_reader_get_num_channels(wr);
            int rate = wave_reader_get_sample_rate(wr);
            int nSamples = wave_reader_get_num_samples(wr);

            if (format != 1 || nChannels != 1 || !(rate == 22050 || rate == 44100)) {
                printf("Input '%s' must be 22050/44100 Hz Mono, freq=%d channels=%d\n", fname, rate, nChannels);
                return 100;
            }

            int16_t* data = new int16_t[nSamples];
            wave_reader_get_samples(wr, nSamples, data);
            if (rate == 44100) {
                data = covert44to22(nSamples, data, &nSamples);
            }

            int32_t stages[3] = { 0 };

            uint8_t* compressed = 0;
            uint32_t nCompressed = 0;
            int64_t e12 = 0;
            int32_t* stereo = compressAndTest(data, nSamples,
                codec, &compressed, &nCompressed, &e12);

            image.addFile(stdfname.c_str(), compressed, nCompressed,
                codec == wav12::ExpanderAD4::Codec::BIT8, e12);

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
