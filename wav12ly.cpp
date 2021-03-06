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

int sign(int a) {
    if (a > 0) return 1;
    if (a < 0) return -1;
    return 0;
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

void runTest(const int16_t* samplesIn, int nSamplesIn, int tolerance)
{
    const int SIZE[4] = { 16, 32, 64, 128 };

    for (int codec = 4; codec <= 8; codec += 4) {
        for (int s = 0; s < 4; ++s) {
            for (int loop = 0; loop <= 1; ++loop) {
                for (int nChannels = 1; nChannels <= 2; ++nChannels) {

                    int targetSize = SIZE[s];

                    // Construct a buf & wrapping buf to dectect overrun
                    int32_t* outBuf = new int32_t[targetSize * 2 + 2];
                    int32_t* stereo = outBuf + 1;
                    outBuf[0] = 37;
                    outBuf[1 + targetSize * 2] = 53;
                    for (int i = 0; i < targetSize; ++i) {
                        stereo[i * 2 + 0] = -111;
                        stereo[i * 2 + 1] = 112;
                    }

                    // Compression
                    uint32_t nCompressed = 0;
                    uint8_t* compressed = new uint8_t[nSamplesIn];
                    int32_t error = 0;
                    ExpanderAD4::compress(codec, samplesIn, nSamplesIn, compressed, &nCompressed, S4ADPCM::getTable(codec, 0), &error);

                    // Decompress
                    MemStream memStream0(compressed, nCompressed);
                    MemStream memStream1(compressed, nCompressed);
                    memStream0.set(0, nCompressed);
                    memStream1.set(0, nCompressed);
                    ExpanderAD4 expander[2];
                    expander[0].init(&memStream0, codec, 0);
                    expander[1].init(&memStream1, codec, 0);

                    bool loopArr[2] = { loop > 0, loop > 0 };
                    int volume[2] = { 256, 256 };
                    ExpanderAD4::fillBuffer(stereo, targetSize, expander, nChannels, loopArr, volume, true);

                    // Verify
                    // Memory overrun:
                    W12ASSERT(outBuf[0] == 37);
                    W12ASSERT(outBuf[1 + targetSize * 2] == 53);
                    // Verify trend, but not sound quality
                    int localT = tolerance * nChannels;
                    if (codec == 8) localT *= 2;

                    if (loop) {
                        for (int i = 0; i < targetSize; i++) {
                            int srcIndex = i % nSamplesIn;
                            if (srcIndex > 0) {
                                int deltaOut = stereo[srcIndex * 2 + 1] / 65536 - stereo[(srcIndex - 1) * 2 + 0] / 65536;
                                int deltaIn = samplesIn[srcIndex] - samplesIn[srcIndex - 1];
                                W12ASSERT(abs(deltaOut - deltaIn) < localT);
                            }
                        }
                    }
                    else {
                        int nCheck = nSamplesIn <= targetSize ? nSamplesIn : targetSize;
                        for (int i = 0; i < nCheck; i++) {
                            int srcIndex = i;
                            if (srcIndex > 0) {
                                int deltaOut = stereo[srcIndex * 2 + 1] / 65536 - stereo[(srcIndex - 1) * 2 + 0] / 65536;
                                int deltaIn = samplesIn[srcIndex] - samplesIn[srcIndex - 1];
                                W12ASSERT(abs(deltaOut - deltaIn) < localT);
                            }
                        }
                        for (int i = nCheck; i < targetSize; ++i) {
                            W12ASSERT(stereo[i * 2 + 0] == 0);
                            W12ASSERT(stereo[i * 2 + 1] == 0);
                        }
                    }

                    // Free resources
                    delete[] outBuf;
                    delete[] compressed;
                }
            }
        }
    }
}

void generateTest()
{
    static const int NSAMPLES = 1024;
    int16_t samples[NSAMPLES];
    //static const int NSAMPLES = 1024 * 32;
    //int16_t* samples = new int16_t[NSAMPLES];
    ExpanderAD4::generateTestData(NSAMPLES, samples);
    uint8_t compressed[NSAMPLES / 2];
    const int* table = S4ADPCM::getTable(4, 0);
    int32_t err = 0;

    S4ADPCM::State state;
    int nCompressed = S4ADPCM::encode4(samples, NSAMPLES, compressed, &state, table, &err);
    int root = (int)sqrtf((float)err);
    assert(nCompressed == NSAMPLES / 2);
    assert(root < 400);
    /*
    int32_t stereo[NSAMPLES * 2];
    state = S4ADPCM::State();
    S4ADPCM::decode4(compressed, nCompressed, 64, false, stereo, &state, table);

    saveOut("testwav.wav", stereo, NSAMPLES);
    delete[] samples;
    */
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
    int16_t TEST_1[12] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110 };
    int16_t TEST_2[12] = { 0, 10, -20, 30, -40, 50, -60, 70, -80, 90, -100, 110 };
    int16_t TEST_3[12] = { 0, 0, 0, 0, 10, 10, 10, -10, -10, -10, -10 };
    int16_t TEST_4[18] = { 0, -100, -200, -300, -400, -300, -200, -100, 0, 100, 200, 300, 400, 300, 200, 100, 0, -100 };

    runTest(TEST_1, 12, 50);
    runTest(TEST_2, 12, 300);
    runTest(TEST_3, 12, 30);
    runTest(TEST_4, 18, 200);

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
    runTest(wr, 8);
    wave_reader_close(wr);
    return 0;
}


bool runTest(wave_reader* wr, int compressBits)
{
    int format = wave_reader_get_format(wr);
    int nChannels = wave_reader_get_num_channels(wr);
    int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    int16_t* data = new int16_t[nSamples];
    wave_reader_get_samples(wr, nSamples, data);

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

            int bits = 4;
            fileElement->QueryIntAttribute("compression", &bits);
            int loopFade = 0;
            fileElement->QueryIntAttribute("loopFade", &loopFade);

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

            int16_t* data = new int16_t[nSamples + 1];  // allocate extra in case we need to tack on.
            wave_reader_get_samples(wr, nSamples, data);
            if (rate == 44100) {
                data = covert44to22(nSamples, data, &nSamples);
            }
            if (nSamples & 1) {
                data[nSamples] = data[nSamples - 1];
                nSamples++;
            }

            int32_t err = 0;
            int table = 0;
            int32_t bestE = INT32_MAX;
            uint8_t* compressed = new uint8_t[nSamples];
            uint32_t nCompressed;

            for (int i = 0; i < S4ADPCM::N_TABLES; ++i) {
                int32_t error = 0;
                wav12::ExpanderAD4::compress(bits, data, nSamples, compressed, &nCompressed, S4ADPCM::getTable(bits, i), &error);

                if (error < bestE) {
                    bestE = error;
                    table = i;
                }
            }

            int32_t* stereo = compressAndTest(data, nSamples, bits, table, compressed, &nCompressed, &err);
            std::string f = fname + std::string(".test.wav");
            saveOut(f.c_str(), stereo, nSamples);

            image.addFile(stdfname.c_str(), compressed, nCompressed, bits == 8, table, err);

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
