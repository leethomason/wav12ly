#include <assert.h>
#include <string.h>
#include <string>

#include "memimage.h"
extern "C" { 
#include "wave_reader.h" 
}
#include "./wav12/compress.h"

template<class T>
T miMin(T a, T b) { return (a < b) ? a : b; }

MemImageUtil::MemImageUtil()
{
    assert(sizeof(MemImage) == 1024);
    dataVec = new uint8_t[DATA_VEC_SIZE];
    memset(dataVec, 0, DATA_VEC_SIZE);
    currentPos = sizeof(MemImage);  // move the write head past the header.
}


MemImageUtil::~MemImageUtil()
{
    delete[] dataVec;
}


void MemImageUtil::addDir(const char* name)
{
    currentDir++;
    assert(currentDir < MemImage::NUM_DIR);
    MemImage* image = (MemImage*)dataVec;
    strncpy(image->dir[currentDir].name, name, MemUnit::NAME_LEN);
    image->dir[currentDir].offset = currentFile + 1;
}


void MemImageUtil::addFile(const char* name, void* data, int size)
{
    assert(currentDir >= 0);
    currentFile++;
    assert(currentFile < MemImage::NUM_FILES);
    
    MemImage* image = (MemImage*)dataVec;
    image->dir[currentDir].size += 1;
    strncpy(image->file[currentFile].name, name, MemUnit::NAME_LEN);
    image->file[currentFile].offset = currentPos;
    image->file[currentFile].size = size;

    memcpy(dataVec + currentPos, data, size);
    currentPos += size;
}


void MemImageUtil::write(const char* name)
{
    FILE* fp = fopen(name, "wb");
    fwrite(dataVec, currentPos, 1, fp);
    fclose(fp);
}

void MemImageUtil::writeText(const char* name)
{
    FILE* fp = fopen(name, "w");

    fprintf(fp, "%d\n", currentPos);
    int line = 0;
    for (uint32_t i = 0; i < currentPos; ++i) {
        fprintf(fp, "%02x", dataVec[i]);
        line++;
        if (line == 64) {
            fprintf(fp, "\n");
            line = 0;
        }
    }
    if (line) {
        fprintf(fp, "\n");
    }
    fclose(fp);
}

void MemImageUtil::dumpConsole()
{
    uint32_t totalUncompressed = 0, totalSize = 0;
    const MemImage* image = (const MemImage*)dataVec;

    for (int d = 0; d < MemImage::NUM_DIR; ++d) {
        uint32_t dirTotal = 0;

        if (image->dir[d].name[0]) {
            char dirName[9] = { 0 };
            strncpy(dirName, image->dir[d].name, 8);
            printf("Dir: %s\n", dirName);
             
            for (unsigned f = 0; f < image->dir[d].size; ++f) {
                const MemUnit& fileUnit = image->file[image->dir[d].offset + f];
                char fileName[9] = { 0 };
                strncpy(fileName, fileUnit.name, 8);

                const wav12::Wav12Header* header =
                    (const wav12::Wav12Header*)(dataVec + fileUnit.offset);

                // Verify!
                bool okay = true;
                {
                    std::string path = std::string(dirName) 
                        + "/" + std::string(fileName) + std::string(".wav");

                    int16_t* wav = 0;
                    int nSamples = 0;
                    {
                        wave_reader_error error = WR_NO_ERROR;
                        wave_reader* wr = wave_reader_open(path.c_str(), &error);
                        assert(error == WR_NO_ERROR);
                        nSamples = wave_reader_get_num_samples(wr);
                        wav = new int16_t[nSamples];
                        wave_reader_get_samples(wr, nSamples, wav);
                        wave_reader_close(wr);
                    }

                    assert(fileUnit.size == header->lenInBytes + sizeof(wav12::Wav12Header));
                    assert(nSamples == header->nSamples);
                    
                    wav12::MemStream memStream(dataVec, DATA_VEC_SIZE);
                    memStream.set(fileUnit.offset + sizeof(wav12::Wav12Header), header->lenInBytes);
                    
                    static const int STEREO_SAMPLES = 256;
                   // int32_t stereo[STEREO_SAMPLES * 2];

/*                    wav12::ExpanderV expander;
                    expander.init(&memStream);
                    for (int i = 0; i < nSamples; i += STEREO_SAMPLES) {
                        int n = miMin(STEREO_SAMPLES, nSamples - i);
                        expander.expand(stereo, n, 1, false);

                        for (int j = 0; j < n; ++j) {
                            int diff = abs(stereo[j * 2] - wav[i + j]);
                            if (diff >= 16) {
                                assert(false);
                                okay = false;
                            }
                        }
                    }
                    */
                    delete[] wav;
                }

                printf("   %8s at %8d size=%6d (%3dk) ratio=%5.1f %s\n", 
                    fileName, 
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    100.0f * float(header->lenInBytes) / (float)(header->nSamples*2),
                    okay ? "ok" : "ERROR" );

                totalUncompressed += header->nSamples * 2;
                totalSize += header->lenInBytes;
                dirTotal += header->lenInBytes;
            }
        }
        if (dirTotal)
            printf("  Dir total=%dk\n", dirTotal / 1024);
    }
    size_t totalImageSize = sizeof(MemImage) + currentPos;
    printf("Overall ratio=%5.2f\n", (float)totalSize / (float)(totalUncompressed));
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
}

