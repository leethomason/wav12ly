#ifndef MEMORY_IMAGE_INCLUDE
#define MEMORY_IMAGE_INCLUDE

#include <vector>
#include <stdint.h>

struct MemUnit {
    static const int NAME_LEN = 8;

    char name[NAME_LEN];   // NOT null terminated, but 0-filled.
    uint32_t offset;
    uint32_t size : 24;
    uint32_t shortSample : 1;
    uint32_t reserve : 7;

    uint32_t numSamples() const { return size * 2 - shortSample; }
};

static_assert(sizeof(MemUnit) == 16, "16 byte MemUnit");

struct MemImage {
    static const int NUM_DIR = 4;
    static const int NUM_FILES = 60;
    static const int NUM = NUM_DIR + NUM_FILES;

    MemUnit dir[NUM_DIR];
    MemUnit file[NUM_FILES];
};

class MemImageUtil
{
public:
    MemImageUtil();
    ~MemImageUtil();

    void addDir(const char* name);
    // MSE just used for debugging output.
    void addFile(const char* name, void* data, int size, int nSamples, int mse);
    void dumpConsole();

    void write(const char* name);
    void writeText(const char* name);

private:
    static const int DATA_VEC_SIZE = 4 * 1024 * 1024;   // 4 meg for overflow, experimentation
    uint32_t currentPos = 0;
    uint8_t* dataVec = 0;
    int currentDir = -1;
    int currentFile = -1;
    int mse[MemImage::NUM_FILES];
};

#endif // MEMORY_IMAGE_INCLUDE
