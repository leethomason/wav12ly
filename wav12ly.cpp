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

bool runTest(wave_reader* wr);
int parseXML(const std::vector<std::string>& files, const std::string& inputPath, bool textFile);

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
                ExpanderAD4::compress(samplesIn, nSamplesIn, compressed, &nCompressed, S4ADPCM::getTable(0), &error);

                // Decompress
                MemStream memStream0(compressed, nCompressed);
                MemStream memStream1(compressed, nCompressed);
                memStream0.set(0, nCompressed);
                memStream1.set(0, nCompressed);
                ExpanderAD4 expander[2];
                expander[0].init(&memStream0, 0);
                expander[1].init(&memStream1, 0);

                bool loopArr[2] = { loop > 0, loop > 0 };
                int volume[2] = { 256, 256 };
                ExpanderAD4::fillBuffer(stereo, targetSize, expander, nChannels, loopArr, volume, true);

                // Verify
                // Memory overrun:
                W12ASSERT(outBuf[0] == 37);
                W12ASSERT(outBuf[1 + targetSize * 2] == 53);
                // Verify trend, but not sound quality
                int localT = tolerance * nChannels;

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

void generateTest()
{
    static const int NSAMPLES = 1024;
    int16_t samples[NSAMPLES];
    //static const int NSAMPLES = 1024 * 32;
    //int16_t* samples = new int16_t[NSAMPLES];
    ExpanderAD4::generateTestData(NSAMPLES, samples);
    uint8_t compressed[NSAMPLES / 2];
    const int* table = S4ADPCM::getTable(0);
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

    std::vector<std::string> xmlFiles;
    for (int i = 1; i < argc; ++i) {
        if (strstr(argv[i], ".xml")) {
            xmlFiles.push_back(std::string(argv[i]));
        }
    }
    if (!xmlFiles.empty()) {
        int rc = parseXML(xmlFiles, inputPath, writeText);
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

// For a looping sound, found a good zero point, and
// shift things so the sound sample starts at the zero.
// This allows for much better compression.
int rotateZero(int16_t* data, int nSamples)
{
    int zero = 0;
    int bestE = INT_MAX;
    for (int i = 0; i < nSamples; i++) {

        int left = (i + nSamples - 1) % nSamples;
        int right = (i + 1) % nSamples;

        int e = 2 * abs(data[i]) + abs(data[left]) + abs(data[right]);
        if (e < bestE) {
            bestE = e;
            zero = i;
        }
    }
    int16_t* c = new int16_t[nSamples];
    memcpy(c, data, nSamples * sizeof(int16_t));
    for (int i = 0; i < nSamples; i++) {
        data[i] = c[(i + zero) % nSamples];
    }
    delete[] c;
    return zero;
}

bool runTest(wave_reader* wr)
{
    const int format = wave_reader_get_format(wr);
    const int nChannels = wave_reader_get_num_channels(wr);
    const int rate = wave_reader_get_sample_rate(wr);
    int nSamples = wave_reader_get_num_samples(wr);

    int16_t* data = new int16_t[nSamples + 1];
    wave_reader_get_samples(wr, nSamples, data);

    int bestTable = 0;
    int bestError = INT_MAX;

    if (nSamples & 1) {
        // Force to even.
        data[nSamples] = data[nSamples - 1];
        ++nSamples;
    }
//    int z = rotateZero(data, nSamples);
//    printf("rotate: %d\n", z);

    if (nChannels != 1 || rate != 22050) {
        printf("Can't test nChannels=%d and rate=%d\n", nChannels, rate);
    }
    
    for (int t = 0; t < S4ADPCM::N_TABLES; ++t) {
        // Compression
        uint32_t nCompressed = 0;
        uint8_t* compressed = new uint8_t[nSamples];
        int32_t error = 0;
        const int* table = S4ADPCM::getTable(t);
        ExpanderAD4::compress(data, nSamples, compressed, &nCompressed, table, &error);

        // Decompress
        MemStream memStream0(compressed, nCompressed);
        memStream0.set(0, nCompressed);
        ExpanderAD4 expander;
        expander.init(&memStream0, t);
        const int volume = 256;

        int32_t* stereo = new int32_t[nSamples * 2];
        const bool LOOP = false;
        ExpanderAD4::fillBuffer(stereo, nSamples, &expander, 1, &LOOP, &volume, true);

        const int32_t SHIFT = 65536;

        printf("          First:               Last:\n");
        printf("In  : %6d %6d %6d %6d    %6d %6d %6d %6d\n",
            data[0], data[1], data[2], data[3],
            data[nSamples - 4], data[nSamples - 3], data[nSamples - 2], data[nSamples - 1]);
        printf("Post: %6d %6d %6d %6d    %6d %6d %6d %6d\n",
            stereo[0] / SHIFT, stereo[2] / SHIFT, stereo[4] / SHIFT, stereo[6] / SHIFT,
            stereo[nSamples * 2 - 8] / SHIFT, stereo[nSamples * 2 - 6] / SHIFT, stereo[nSamples * 2 - 4] / SHIFT, stereo[nSamples * 2 - 2] / SHIFT);
        printf("Table=%d Error: %d\n", t, error);

        if (error < bestError) {
            bestError = error;
            bestTable = t;
        }

        if (t == 0) {
            saveOut("testPost.wav", stereo, nSamples);

            int32_t* loopStereo = new int32_t[nSamples * 2 * 4];
            for (int i = 0; i < 4; ++i) {
                memcpy(loopStereo + nSamples * 2 * i, stereo, nSamples * 2 * sizeof(int32_t));
            }
            saveOut("testPostLoop.wav", loopStereo, nSamples * 4);
            delete[] loopStereo;
        }
        delete[] compressed;
        delete[] stereo;
    }
    printf("Best table=%d error=%d", bestTable, bestError);
    delete[] data;
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


uint8_t ParseOneHex(char c)
{
    c = tolower(c);
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    return c - '0';
}

uint8_t ParseOneHex(const char* in)
{
    return ParseOneHex(in[0]) * 16 + ParseOneHex(in[1]);
}

void ParseHex(const char* in, uint8_t* r, uint8_t* g, uint8_t* b)
{
    *r = ParseOneHex(in + 0);
    *g = ParseOneHex(in + 2);
    *b = ParseOneHex(in + 4);
}

int parseXML(const std::vector<std::string>& files, const std::string& inputPath, bool textFile)
{
    MemImageUtil image;
    std::string imageFileName;
    int64_t totalError = 0;

    for (const std::string& filename : files) {
        XMLDocument doc;
        doc.LoadFile(filename.c_str());

        if (doc.Error()) {
            printf("XML error: %s\n", doc.ErrorName());
            return 1;
        }

        const char* rootName = doc.RootElement()->Name();
        if (rootName && strcmp(rootName, "Config") == 0) {

            image.addDir("config");
            int nPalette = 0;

            for (const XMLElement* palElement = doc.RootElement()->FirstChildElement("Palette");
                palElement;
                palElement = palElement->NextSiblingElement("Palette"))
            {
                ++nPalette;
                int font = 0;
                palElement->QueryIntAttribute("font", &font);
                uint8_t bc_r, bc_g, bc_b, ic_r, ic_g, ic_b;
                const char* bc = palElement->Attribute("bc");
                const char* ic = palElement->Attribute("ic");
                ParseHex(bc, &bc_r, &bc_g, &bc_b);
                ParseHex(ic, &ic_r, &ic_g, &ic_b);
                image.addConfig(font, bc_r, bc_g, bc_b, ic_r, ic_g, ic_b);
            }
            assert(nPalette == 8);
        }
        else {
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
                const char* post = dirElement->Attribute("post");
                std::string postPath;
                if (post) {
                    postPath = post;
                    postPath.append("/");
                }

                if (!imageFileName.empty()) {
                    imageFileName += "_";
                }
                imageFileName += fontName;

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

                    int loopFade = 0;
                    fileElement->QueryIntAttribute("loopFade", &loopFade);
                    bool rotateToZero = false;
                    fileElement->QueryBoolAttribute("looping", &rotateToZero);

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
                    if (rotateToZero) {
                        int r = rotateZero(data, nSamples);
                        printf("%s rotated %d samples.\n", fname, r);
                    }

                    int32_t err = 0;
                    int table = 0;
                    int32_t bestE = INT32_MAX;
                    uint8_t* compressed = new uint8_t[nSamples];
                    uint32_t nCompressed;

                    for (int i = 0; i < S4ADPCM::N_TABLES; ++i) {
                        int32_t error = 0;
                        wav12::ExpanderAD4::compress(data, nSamples, compressed, &nCompressed, S4ADPCM::getTable(i), &error);

                        if (error < bestE) {
                            bestE = error;
                            table = i;
                        }
                    }
                    totalError += bestE * nSamples;

                    int32_t* stereo = compressAndTest(data, nSamples, table, compressed, &nCompressed, &err);
                    if (post) {
                        std::string f = postPath + fname;
                        saveOut(f.c_str(), stereo, nSamples);
                    }
                    image.addFile(stdfname.c_str(), compressed, nCompressed, table, err);

                    delete[] compressed;
                    delete[] data;
                    delete[] stereo;
                    wave_reader_close(wr);
                }
            }
        }
    }

    image.dumpConsole();
    printf("TotalError = %lld\n", totalError / 1'000'000);
    //image.write("memimage.bin");
    if (textFile) {
        image.writeText((imageFileName + ".txt").c_str());
    }
    return 0;
}

/*
--> ob4.xml hero.xml config_blue.xml 

hum01.wav rotated 94443 samples.
hum01.wav rotated 59017 samples.
Dir: ob4
     blst01 at     1024 size= 12917 ( 12k) table=2 ave-err=  741.8
     blst02 at    13941 size= 16664 ( 16k) table=0 ave-err=  862.2
     blst03 at    30605 size= 20994 ( 20k) table=2 ave-err=  742.6
     blst05 at    51599 size= 16816 ( 16k) table=0 ave-err=  766.3
     blst06 at    68415 size= 12855 ( 12k) table=0 ave-err=  818.9
     clsh01 at    81270 size= 12391 ( 12k) table=0 ave-err=  992.8
     clsh02 at    93661 size= 11025 ( 10k) table=1 ave-err= 1012.5
     clsh03 at   104686 size= 14724 ( 14k) table=0 ave-err=  911.8
     clsh04 at   119410 size= 11714 ( 11k) table=0 ave-err=  810.5
     clsh05 at   131124 size= 11025 ( 10k) table=0 ave-err=  978.7
     clsh06 at   142149 size= 11025 ( 10k) table=1 ave-err=  995.2
     clsh07 at   153174 size= 11025 ( 10k) table=0 ave-err=  944.1
     clsh08 at   164199 size= 11779 ( 11k) table=0 ave-err= 1111.6
      hum01 at   175978 size=234741 (229k) table=0 ave-err=    7.9
       in01 at   410719 size=  6052 (  5k) table=0 ave-err= 2966.5
      out01 at   416771 size= 14471 ( 14k) table=1 ave-err= 1007.3
   swingh01 at   431242 size=140569 (137k) table=0 ave-err=   82.3
   swingh02 at   571811 size=140569 (137k) table=0 ave-err=   82.0
   swingl01 at   712380 size=140569 (137k) table=0 ave-err=   81.7
   swingl02 at   852949 size=140569 (137k) table=0 ave-err=   81.3
  Dir total=969k
Dir: hero
     blst01 at   993518 size=  7816 (  7k) table=1 ave-err=  769.0
     blst02 at  1001334 size=  5836 (  5k) table=0 ave-err=  864.8
     blst03 at  1007170 size=  6600 (  6k) table=0 ave-err=  790.4
     blst04 at  1013770 size=  5701 (  5k) table=0 ave-err=  879.0
     blst05 at  1019471 size=  7169 (  7k) table=2 ave-err=  977.9
     blst06 at  1026640 size=  6713 (  6k) table=0 ave-err=  774.7
     blst07 at  1033353 size=  8526 (  8k) table=0 ave-err=  911.1
     blst08 at  1041879 size=  7434 (  7k) table=0 ave-err=  841.9
     clsh01 at  1049313 size=  6432 (  6k) table=1 ave-err= 1643.3
     clsh02 at  1055745 size=  6399 (  6k) table=1 ave-err= 1249.7
     clsh03 at  1062144 size=  8711 (  8k) table=2 ave-err= 1148.0
     clsh04 at  1070855 size=  5698 (  5k) table=1 ave-err=  918.9
     clsh05 at  1076553 size=  3783 (  3k) table=0 ave-err= 1115.9
     clsh06 at  1080336 size=  3446 (  3k) table=1 ave-err=  928.3
     clsh07 at  1083782 size=  4994 (  4k) table=1 ave-err= 1118.8
     clsh08 at  1088776 size=  5261 (  5k) table=2 ave-err= 1279.8
     clsh09 at  1094037 size=  5513 (  5k) table=2 ave-err= 1138.4
     clsh10 at  1099550 size=  5825 (  5k) table=2 ave-err= 1100.0
     clsh13 at  1105375 size=  5698 (  5k) table=0 ave-err=  956.2
     clsh14 at  1111073 size=  5913 (  5k) table=1 ave-err= 1354.5
     clsh15 at  1116986 size=  5513 (  5k) table=1 ave-err= 1293.3
     clsh16 at  1122499 size=  5577 (  5k) table=1 ave-err= 1147.0
      hum01 at  1128076 size=140569 (137k) table=0 ave-err=   13.2
       in01 at  1268645 size=  6432 (  6k) table=1 ave-err= 3981.6
       in02 at  1275077 size=  6432 (  6k) table=1 ave-err= 4041.9
      out01 at  1281509 size= 11169 ( 10k) table=1 ave-err= 2465.5
      out02 at  1292678 size= 11025 ( 10k) table=1 ave-err= 1935.6
   swingh01 at  1303703 size=159863 (156k) table=0 ave-err=  342.3
   swingh03 at  1463566 size=132300 (129k) table=0 ave-err=  270.8
   swingl01 at  1595866 size=126788 (123k) table=0 ave-err=  291.2
   swingl03 at  1722654 size=121275 (118k) table=0 ave-err=  266.7
  Dir total=830k
config
  font=0 bc=0088ff ic=44ccff
  font=0 bc=00ff00 ic=00ffa0
  font=1 bc=c000ff ic=80a080
  font=1 bc=ff0000 ic=a08000
  font=1 bc=ff6000 ic=808000
  font=0 bc=ffff00 ic=00ff88
  font=1 bc=80a080 ic=30a0a0
  font=0 bc=00ff44 ic=00ffaa
Overall ratio= 0.25
Image size=1844953 bytes, 1801 k
Directory name hash=fa135ec3
*/

/*
Format = 1 channels = 1 rate = 22050
Running basic tests on 'hum01.wav'
rotate: 246413
First 4 : -7 - 1     10     58  Post : -7 - 2     11     52
Last 4 : 51      1     11      1  Post : 54      4     10     16
Error : 202
*/  