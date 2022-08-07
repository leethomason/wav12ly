#ifndef MEMORY_IMAGE_INCLUDE
#define MEMORY_IMAGE_INCLUDE

#include <vector>
#include <stdint.h>

struct MemUnit {
    static const int NAME_LEN = 8;

    char name[NAME_LEN];   // NOT null terminated, but 0-filled.
    uint32_t offset;
    uint32_t size : 24;     // if needed, an extra sample is added so that size==nSamples
    uint32_t table : 4;     // 0-15 to select table

    uint32_t numSamples() const { return size * 2; }
};

static_assert(sizeof(MemUnit) == 16, "16 byte MemUnit");

struct ConfigUnit {
    char name[MemUnit::NAME_LEN];
    uint8_t soundFont, bc_r, bc_b, bc_g;
    uint8_t ic_r, ic_b, ic_g, reserve;
};

static_assert(sizeof(ConfigUnit) == 16, "16 byte ConfigUnit");
static_assert(sizeof(ConfigUnit) == sizeof(MemUnit), "MemUnit and ConfigUnit should be the same size");

struct MemImage {
    static const int NUM_DIR = 4;
    static const int NUM_FILES = 60;
    static const int NUM = NUM_DIR + NUM_FILES;

    MemUnit unit[NUM];
};

class MemImageUtil
{
public:
    MemImageUtil();
    ~MemImageUtil();

    void addDir(const char* name);
    void addFile(const char* name, void* data, int size, int table, int64_t e12);
    void addConfig(uint8_t font, uint8_t bc_r, uint8_t bc_g, uint8_t bc_b, uint8_t ic_r, uint8_t ic_g, int8_t ic_b);
    void dumpConsole();

    void write(const char* name);
    void writeText(const char* name);

private:
    static const int DATA_VEC_SIZE = 16 * 1024 * 1024;
    uint32_t currentPos = 0;
    uint8_t* dataVec = 0;
    int numDir = 0;
    int numFile = 0;
    int64_t e12[MemImage::NUM_FILES];
};

#endif // MEMORY_IMAGE_INCLUDE
