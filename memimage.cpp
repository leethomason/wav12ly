#include <assert.h>
#include <string.h>

#include "memimage.h"
#include "wavutil.h"

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
    memset(mse, 0, sizeof(int) * MemImage::NUM_FILES);
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


void MemImageUtil::addFile(const char* name, void* data, int size, int nSamples, int _mse)
{
    assert(size * 2 == nSamples || size * 2 - 1 == nSamples);
    assert(currentDir >= 0);
    currentFile++;
    assert(currentFile < MemImage::NUM_FILES);
    
    MemImage* image = (MemImage*)dataVec;
    image->dir[currentDir].size += 1;
    strncpy(image->file[currentFile].name, name, MemUnit::NAME_LEN);
    image->file[currentFile].offset = currentPos;
    image->file[currentFile].size = size;
    if (size * 2 - 1 == nSamples) {
        image->file[currentFile].shortSample = 1;
    }
    mse[currentFile] = _mse;
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
                int index = image->dir[d].offset + f;
                const MemUnit& fileUnit = image->file[index];
                char fileName[9] = { 0 };
                strncpy(fileName, fileUnit.name, 8);

                printf("   %8s at %8d size=%6d (%3dk) shrt=%d ratio=%5.1f mse=%8d\n",
                    fileName,
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    fileUnit.shortSample,
                    100.0f * float(fileUnit.size) / (float)(fileUnit.numSamples() * 2),
                    mse[index]);

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
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
}

