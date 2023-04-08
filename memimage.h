#ifndef MEMORY_IMAGE_INCLUDE
#define MEMORY_IMAGE_INCLUDE

#include <stdint.h>
#include <vector>

#include "./wav12util/manifest.h"

class MemImageUtil
{
public:
    MemImageUtil();
    ~MemImageUtil();

    void addDir(const char* name);
    void addFile(const char* name, const void* data, int size, int table, int predictor, int32_t e12);
    void addConfig(uint8_t font, uint8_t bc_r, uint8_t bc_g, uint8_t bc_b, uint8_t ic_r, uint8_t ic_g, int8_t ic_b);
    void dumpConsole();
    int getNumFiles() const {
        return numFile;
    }
    int getNumDirs() const {
        return numDir;
    }

    void write(const char* name);
    void writeText(const char* name);

    static bool Test();

private:
    static const int MEMORY_SIZE = 2'000'000;   // 2 million or...bigger? 2 * 1024 * 1024??
    int numDir = 0;
    int numFile = 0;
    int addr = MemImage::SIZE;      // points to the beginning of th eheap.
    int32_t e12[MemImage::NUM];
    MemImage* image = 0;            // alias to dataVac
    uint8_t* dataVec;               // The entire 2mb image. The MemImage is at the beginning.
};

#endif // MEMORY_IMAGE_INCLUDE
