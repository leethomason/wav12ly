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
}

#include "tinyxml2.h"
#include "memimage.h"

#include "./wav12/compress.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr, int shift);
int parseXML(const char* filename, bool textFile);

void basicTest()
{
    static const int NSAMPLES = 11;
    static const int16_t samples[NSAMPLES] = { -1000, -800, -600, -400, -200, -1, 2, 0, 1, 200, 400 };
    uint32_t nCompressed = 0;
    uint8_t* compressed = 0;
    wav12::compress(samples, NSAMPLES, &compressed, &nCompressed);

    static const int BUFSIZE = 32;
    uint8_t buffer[BUFSIZE];

    int32_t newData[NSAMPLES *2];
    MemStream memStream(compressed, nCompressed);
    Expander expander(buffer, BUFSIZE);
    expander.init(&memStream, NSAMPLES, 1);
    expander.expand2(newData, NSAMPLES, 1);
    for (int i = 0; i < NSAMPLES; i++) {
        assert(abs(newData[i * 2] - samples[i]) < 16);
    }
}

int main(int argc, const char* argv[])
{
    basicTest();

    if (argc < 2) {
        printf("Usage:\n");
        printf("    wav12 filename                         Runs tests on 'filename'\n");
        printf("    wav12 xmlFile                          Creates memory image.\n");
        printf("Options:\n");
        printf("    -t, write text file.\n");
        return 1;
    }
    if (strstr(argv[1], ".xml")) {
        bool textFile = false;
        if (argc == 3 && strcmp(argv[2], "-t") == 0) {
            textFile = true;
        }
        int rc = parseXML(argv[1], textFile);
        printf("Image generation return code=%d\n", rc);
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

    runTest(wr, 0); 
    runTest(wr, 1);
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
    wav12::compress(data, nSamples, &compressed, &nCompressed);

    static const int BUFSIZE = 32;
    uint8_t buffer[BUFSIZE];

    int32_t* stereoData = new int32_t[nSamples*2];
    MemStream memStream(compressed, nCompressed);
    if (!compress) {
        // put the raw stream in:
        memStream.init((const uint8_t*)data, nSamples * 2);
    }
    Expander expander(buffer, BUFSIZE);
    expander.init(&memStream, nSamples, compress);

    static const int STEP = 1024;
    for (int i = 0; i < nSamples; i += STEP) {
        int n = std::min(STEP, nSamples - i);
        expander.expand2(stereoData + i*2, n, 1);
    }

    bool okay = true;
    for (int i = 0; i < nSamples; i++) {
        if (abs(data[i] - stereoData[i*2]) >= error) {
            assert(false);
            okay = false;
        }
    }
    printf("Test full stream: %s\n", okay ? "PASS" : "FAIL");
    if (!okay) return false;
    delete[] data;
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
            wav12::compress(data, nSamples, &compressed, &nCompressed);
            bool isCompressed = fileElement->BoolAttribute("compress", true);

            Wav12Header header;
            memset(&header, 0, sizeof(header));
            header.id[0] = 'w';
            header.id[1] = 'l';
            header.id[2] = '1';
            header.id[3] = '2';
            header.lenInBytes = isCompressed ? nCompressed : nSamples * 2;
            header.nSamples = nSamples;
            header.format = isCompressed ? 1 : 0;

            uint8_t* mem = new uint8_t[sizeof(Wav12Header) + header.lenInBytes];
            memcpy(mem, &header, sizeof(Wav12Header));
            if (isCompressed)
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