#include <assert.h>
#include <string.h>

#include "memimage.h"
#include "wavutil.h"

extern "C" { 
#include "wave_reader.h" 
}
#include "./wav12/expander.h"

template<class T>
T miMin(T a, T b) { return (a < b) ? a : b; }

MemImageUtil::MemImageUtil()
{
    assert(sizeof(MemImage) == 1024);
    dataVec = new uint8_t[DATA_VEC_SIZE];
    memset(dataVec, 0, DATA_VEC_SIZE);
    currentPos = sizeof(MemImage);  // move the write head past the header.
    memset(mse, 0, sizeof(int) * MemImage::NUM_FILES);
}


MemImageUtil::~MemImageUtil()
{
    delete[] dataVec;
}


void MemImageUtil::addDir(const char* name)
{
    assert(numDir < MemImage::NUM_DIR);
    MemImage* image = (MemImage*)dataVec;
    strncpy(image->unit[numDir].name, name, MemUnit::NAME_LEN);
    image->unit[numDir].offset = MemImage::NUM_DIR + numFile;
    numDir++;
}


void MemImageUtil::addFile(const char* name, void* data, int size, bool use8Bit, int _mse)
{   
    assert(numDir > 0);
    assert(numFile < MemImage::NUM_FILES);

    MemImage* image = (MemImage*)dataVec;
    image->unit[numDir-1].size += 1;
    int index = MemImage::NUM_DIR + numFile;
    strncpy(image->unit[index].name, name, MemUnit::NAME_LEN);
    image->unit[index].offset = currentPos;
    image->unit[index].size = size;
    if (use8Bit) {
        image->unit[index].is8Bit = 1;
    }
    mse[numFile] = _mse;
    memcpy(dataVec + currentPos, data, size);
    currentPos += size;
    numFile++;
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
    int64_t totalMSE4 = 0;
    int64_t totalMSE8 = 0;
    int count4 = 0;
    int count8 = 0;

    for (int d = 0; d < MemImage::NUM_DIR; ++d) {
        uint32_t dirTotal = 0;

        if (image->unit[d].name[0]) {
            char dirName[9] = { 0 };
            strncpy(dirName, image->unit[d].name, 8);
            printf("Dir: %s\n", dirName);

            for (unsigned f = 0; f < image->unit[d].size; ++f) {
                int index = image->unit[d].offset + f;
                const MemUnit& fileUnit = image->unit[index];
                char fileName[9] = { 0 };
                strncpy(fileName, fileUnit.name, 8);

                printf("   %8s at %8d size=%6d (%3dk) ratio=%5.1f use8Bit=%d mse=%8d\n",
                    fileName,
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    100.0f * float(fileUnit.size) / (float)(fileUnit.numSamples() * 2),
                    fileUnit.is8Bit,
                    mse[index - MemImage::NUM_DIR]);

                if (fileUnit.is8Bit) {
                    totalMSE8 += mse[index - MemImage::NUM_DIR];
                    count8++;
                }
                else {
                    totalMSE4 += mse[index - MemImage::NUM_DIR];
                    ++count4;
                }
                totalUncompressed += fileUnit.numSamples() * 2;
                totalSize += fileUnit.size;
                dirTotal += fileUnit.size;
            }
        }
        if (dirTotal)
            printf("  Dir total=%dk\n", dirTotal / 1024);
    }
    size_t totalImageSize = sizeof(MemImage) + currentPos;
    printf("Overall ratio=%5.2f\n", (float)totalSize / (float)(totalUncompressed));
    if (count4)
        printf("Ave 4-bit mse=%dK\n", (int)(totalMSE4 / (1000 * count4)));
    if (count8)
        printf("Ave 8-bit mse=%dK\n", (int)(totalMSE8 / (1000 * count8)));
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
}

