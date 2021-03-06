#include <assert.h>
#include <string.h>
#include <string>

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
    memset(e12, 0, sizeof(int64_t) * MemImage::NUM_FILES);
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


void MemImageUtil::addFile(const char* name, void* data, int size, bool use8Bit, int table, int64_t _e12)
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
    image->unit[index].table = table;
    e12[numFile] = _e12;
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

    static const int STEP = 256;
    char cBuf[STEP*2];

    for (uint32_t i = 0; i < currentPos; i += STEP) {
        int n = STEP;
        if (i + STEP > currentPos)
            n = currentPos - i;
        encodeBase64(dataVec + i, n, cBuf, true);
        fprintf(fp, "%s\n", cBuf);
    }
    fclose(fp);
}

void MemImageUtil::dumpConsole()
{
    uint32_t totalUncompressed = 0, totalSize = 0;
    const MemImage* image = (const MemImage*)dataVec;
    
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

                printf("   %8s at %8d size=%6d (%3dk) table=%d %s ave-err=%7.1f\n",
                    fileName,
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    fileUnit.table,
                    fileUnit.is8Bit ? "8b" : "4b",
                    sqrtf((float)e12[index - MemImage::NUM_DIR]));

                totalUncompressed += fileUnit.numSamples() * 2;
                totalSize += fileUnit.size;
                dirTotal += fileUnit.size;
            }
        }
        if (dirTotal)
            printf("  Dir total=%dk\n", dirTotal / 1024);
    }

    uint32_t dirHash = 0;
    for (int i = 0; i < MemImage::NUM_DIR; i++) {
        dirHash = hash32(image->unit[i].name, image->unit[i].name + MemUnit::NAME_LEN, dirHash);
    }

    size_t totalImageSize = sizeof(MemImage) + currentPos;
    printf("Overall ratio=%5.2f\n", (float)totalSize / (float)(totalUncompressed));
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
    printf("Directory name hash=%x\n", dirHash);
}

