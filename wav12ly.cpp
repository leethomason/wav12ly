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
#include <vector>
#include <limits>

extern "C" {
#include "wave_reader.h"
#include "wave_writer.h"
}

#include "tinyxml2.h"
#include "memimage.h"
#include "wavutil.h"
#include "codec.h"
#include "enkits/TaskScheduler.h"

#include "./wav12/expander.h"

using namespace wav12;
using namespace tinyxml2;

bool runTest(wave_reader* wr);
int parseXML(const std::vector<std::string>& files, const std::string& inputPath, bool textFile);

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

void compressAndCalcErrorADPCM(const int16_t* samples, int nSamples, int32_t* aveError2)
{
    int nCompressed = nSamples / 2;
    uint8_t* compressed = new uint8_t[nCompressed];

    CodecState state = { 0, 0 };
    encodeADPCM(&state, samples, nSamples, compressed);

    int16_t* mono = new int16_t[nSamples];
    state.index = 0;
    state.valprev = 0;
    decodeADPCM(&state, compressed, nSamples, mono);

    if (aveError2) {
        int64_t error2 = 0;
        for (int i = 0; i < nSamples; ++i) {
            int16_t s0 = samples[i];
            int16_t s1 = mono[i];
            int64_t d = int64_t(s0) - int64_t(s1);
            error2 += d * d;
        }
        *aveError2 = int32_t(error2 / nSamples);
    }

    delete[] compressed;
    delete[] mono;
}

void printTable(const int* t)
{
    printf("[%d, %d, %d, %d, %d, %d, %d, %d, %d]",
        t[0],
        t[1],
        t[2],
        t[3],
        t[4],
        t[5],
        t[6],
        t[7],
        t[8]);
}

void optimizeTable(const int16_t* samples, int nSamples)
{
    W12ASSERT((nSamples & 1) == 0);
    int nCompressed = nSamples / 2;

    int table[S4ADPCM::TABLE_SIZE] = { -1, 0, 0, 0, 1, 1, 1, 2, 2 };
    int32_t* stereo = new int32_t[nSamples * 2];
    uint8_t* compressed = new uint8_t[nCompressed];
    int bestError = INT_MAX;

    for(int bit=0; bit<512; ++bit) {
        int b = -2;
        for(int i=0; i<9; ++i) {
            if (bit & (1 << i)) {
                ++b;
            }
            table[i] = b;
        }
        if (table[8] < 1)
            continue;

        S4ADPCM::State state(table, S4ADPCM::State::PREDICTOR);
        S4ADPCM::encode4(samples, nSamples, compressed, &state);

        MemStream memStream0(compressed, nCompressed);
        memStream0.set(0, nCompressed);
        ExpanderAD4 expander;
        expander.init(&memStream0, 0, S4ADPCM::State::PREDICTOR);
        const int volume = 256;
        bool loop = false;

        ExpanderAD4::fillBuffer(stereo, nSamples, &expander, 1, &loop, &volume, true);

        int64_t error2 = 0;
        for (int i = 0; i < nSamples; ++i) {
            int16_t s0 = samples[i];
            int16_t s1 = int16_t(stereo[i * 2] / 65536);
            int64_t d = int64_t(s0) - int64_t(s1);
            error2 += d * d;
        }
        int32_t aveError2 = int32_t(error2 / nSamples);
        if (aveError2 < bestError) {
            bestError = aveError2;

            printf("ave-err=%d ", bestError);
            printTable(table);
            printf("\n");
        }
    }
    delete[] stereo;
    delete[] compressed;
}

#define USE_MT() 1

struct CompressTask : enki::ITaskSet
{
    EncodedStream es;
    const int16_t* samples = 0;
    int nSamples = 0;
    int table = 0;
    int predictor = 0;
    void ExecuteRange(enki::TaskSetPartition range_, uint32_t threadnum_) override {
        this->es = compressS4(samples, nSamples, table, predictor);
    }
};

EncodedStream compressGroup(const int16_t* samples, int nSamples)
{
    static constexpr int32_t N = S4ADPCM::N_TABLES * S4ADPCM::State::N_PREDICTOR;
#if USE_MT()
    CompressTask esArr[N];

    static enki::TaskScheduler taskScheduler;
    static bool initialized = false;
    if (!initialized) {
        taskScheduler.Initialize();
        initialized = true;
    }
#else
    EncodedStream esArr[N];
#endif

    int32_t errADPCM = 0;
    compressAndCalcErrorADPCM(samples, nSamples, &errADPCM);

    for (int table = 0; table < S4ADPCM::N_TABLES; table++) {
        for (int pre = 0; pre < S4ADPCM::State::N_PREDICTOR; pre++) {
            int i = table * S4ADPCM::State::N_PREDICTOR + pre;
#if USE_MT()
            esArr[i].samples = samples;
            esArr[i].nSamples = nSamples;
            esArr[i].table = table;
            esArr[i].predictor = pre;
            taskScheduler.AddTaskSetToPipe(&esArr[i]);
#else
            esArr[i] = compressS4(samples, nSamples, table, pre);
#endif
        }
    }
#if USE_MT()
    taskScheduler.WaitforAll();
#endif

    int32_t bestErr = std::numeric_limits<int32_t>::max();
    int best = 0;
    for (int i = 0; i < N; ++i) {
#if USE_MT()
        const EncodedStream& es = esArr[i].es;
#else
        const EncodedStream& es = esArr[i];
#endif
        printf("Table=%d Predictor=%d Error: %10d ADPCM: %d\n", es.table, es.predictor, es.aveError2, errADPCM);
        if (es.aveError2 < bestErr) {
            bestErr = es.aveError2;
            best = i;
        }
    }
#if USE_MT()
    return std::move(esArr[best].es);
#else
    return std::move(esArr[best]);
#endif
}


EncodedStream compressS4(const int16_t* samples, int nSamples, int table, int32_t predictor)
{
    W12ASSERT((nSamples & 1) == 0);
    int nCompressed = nSamples / 2;

    const int* pTable = S4ADPCM::getTable(table);
    S4ADPCM::State state(pTable, predictor);
    state.predictor = predictor;
    auto compressed = std::make_unique<uint8_t[]>(nCompressed);

    S4ADPCM::encode4(samples, nSamples, compressed.get(), &state);

    auto stereo = std::make_unique<int32_t[]>(nSamples * 2);

    MemStream memStream0(compressed.get(), nCompressed);
    memStream0.set(0, nCompressed);
    ExpanderAD4 expander;
    expander.init(&memStream0, pTable, predictor);
    const int volume = 256;
    bool loop = false;

    ExpanderAD4::fillBuffer(stereo.get(), nSamples, &expander, 1, &loop, &volume, true);

    int64_t error2 = 0;
    for (int i = 0; i < nSamples; ++i) {
        int16_t s0 = samples[i];
        int16_t s1 = int16_t(stereo.get()[i * 2] / 65536);
        int64_t d = int64_t(s0) - int64_t(s1);
        error2 += d * d;
    }
    int32_t aveError2 = int32_t(error2 / nSamples);
    
    return EncodedStream{
        nSamples,
		nCompressed,
		table,
		predictor,
		aveError2,
		std::move(compressed),
		std::move(stereo),
	};
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
                uint32_t nCompressed = nSamplesIn / 2;
                uint8_t* compressed = new uint8_t[nSamplesIn];
                int64_t error = 0;
                S4ADPCM::State state(S4ADPCM::getTable(0), S4ADPCM::State::PREDICTOR);
                S4ADPCM::encode4(samplesIn, nSamplesIn, compressed, &state);

                // Decompress
                MemStream memStream0(compressed, nCompressed);
                MemStream memStream1(compressed, nCompressed);
                memStream0.set(0, nCompressed);
                memStream1.set(0, nCompressed);
                ExpanderAD4 expander[2];
                expander[0].init(&memStream0, S4ADPCM::getTable(0), S4ADPCM::State::PREDICTOR);
                expander[1].init(&memStream1, S4ADPCM::getTable(0), S4ADPCM::State::PREDICTOR);

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
    ExpanderAD4::generateTestData(NSAMPLES, samples);

    EncodedStream es = compressS4(samples, NSAMPLES, 0, S4ADPCM::State::PREDICTOR);

    int root = (int)sqrtf((float)es.aveError2);
    assert(es.nCompressed == NSAMPLES / 2);
    assert(root < 400);
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
    Manifest::Test();
    MemImageUtil::Test();

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
    
    //optimizeTable(data, nSamples);

    EncodedStream es = compressGroup(data, nSamples);

    {
        saveOut("testPost.wav", es.stereo.get(), nSamples);

        int32_t* loopStereo = new int32_t[nSamples * 2 * 4];
        for (int i = 0; i < 4; ++i) {
            memcpy(loopStereo + nSamples * 2 * i, es.stereo.get(), nSamples * 2 * sizeof(int32_t));
        }
        saveOut("testPostLoop.wav", loopStereo, nSamples * 4);
        delete[] loopStereo;
    }
    printf("Best table=%d predictor=%d error=%d\n", es.table, es.predictor, es.aveError2);
    delete[] data;
    return true;
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
    int64_t simpleError = 0;

    for (const std::string& filename : files) {
        XMLDocument doc;
        doc.LoadFile(filename.c_str());

        if (doc.Error()) {
            printf("XML error: %s\n", doc.ErrorName());
            return 1;
        }

        const char* rootName = doc.RootElement()->Name();
        if (rootName && strcmp(rootName, "Config") == 0) {

            int nPalette = 0;

            if (doc.RootElement()->Attribute("name")) {
                imageFileName += "_";
                imageFileName += doc.RootElement()->Attribute("name");
            }
            else {
                imageFileName += "_config";
            }

            for (const XMLElement* palElement = doc.RootElement()->FirstChildElement("Palette");
                palElement;
                palElement = palElement->NextSiblingElement("Palette"))
            {
                MemPalette pal;
                int font = 0;
                palElement->QueryIntAttribute("font", &font);
                pal.soundFont = font;
                const char* bc = palElement->Attribute("bc");
                const char* ic = palElement->Attribute("ic");
                ParseHex(bc, &pal.bladeColor.r, &pal.bladeColor.g, &pal.bladeColor.b);
                ParseHex(ic, &pal.impactColor.r, &pal.impactColor.g, &pal.impactColor.b);
                image.writePalette(nPalette, pal);

                ++nPalette;
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

                    wave_reader_error wrErr = WR_NO_ERROR;
                    wave_reader* wr = wave_reader_open(fullPath.c_str(), &wrErr);
                    if (wrErr != WR_NO_ERROR) {
                        printf("Failed to open: %s\n", fullPath.c_str());
                        return wrErr;
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
                    EncodedStream es = compressGroup(data, nSamples);
                    totalError += int64_t(es.aveError2) * int64_t(nSamples);
                    simpleError += int64_t(es.aveError2);

                    if (post) {
                        std::string f = postPath + fname;
                        saveOut(f.c_str(), es.stereo.get(), nSamples);
                    }
                    image.addFile(stdfname.c_str(), es.compressed.get(), es.nCompressed, es.table, es.predictor, es.aveError2);

                    delete[] data;
                    wave_reader_close(wr);
                }
            }
        }
    }

    image.writeDesc(imageFileName.c_str());

    image.dumpConsole();
    printf("TotalError = %lld  SimpleError = %lld\n", totalError / int64_t(1'000'000'000), simpleError / 1000);
    printf("Num Dirs=%d/%d Files=%d/%d\n", image.getNumDirs(), MemImage::NUM_DIR, image.getNumFiles(), MemImage::NUM_FILES);
    if (image.getNumDirs() > MemImage::NUM_DIR)
        printf("ERROR too many directories\n");
    if (image.getNumFiles() > MemImage::NUM_FILES)
        printf("ERROR too many files\n");
    if (textFile) {
        image.writeText((imageFileName + ".txt").c_str());
    }
    return 0;
}
