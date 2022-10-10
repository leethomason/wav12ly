#include <assert.h>
#include <string.h>
#include <string>

#include "memimage.h"
#include "wavutil.h"

extern "C" { 
#include "wave_reader.h" 
}
#include "./wav12/expander.h"

#define TEST(x) { if (!(x)) { assert(false); return false; }}

MemImageUtil::MemImageUtil()
{
    dataVec = new uint8_t[MEMORY_SIZE];
    memset(dataVec, 0, MEMORY_SIZE);
    image = (MemImage*) dataVec;
    memset(e12, 0, sizeof(e12[0]) * MemImage::NUM_FILES);
}


MemImageUtil::~MemImageUtil()
{
    delete[] dataVec;
}


void MemImageUtil::addDir(const char* name)
{
    assert(numDir < MemImage::NUM_DIR);
    strncpy(image->unit[numDir].name, name, MemUnit::NAME_LEN);
    image->unit[numDir].offset = MemImage::NUM_DIR + numFile;
    numDir++;
}


void MemImageUtil::addFile(const char* name, const void* data, int size, int table, int32_t _e12)
{   
    assert(numDir > 0);
    assert(numFile < MemImage::NUM_FILES);

    image->unit[numDir-1].size += 1;
    int index = MemImage::NUM_DIR + numFile;
    strncpy(image->unit[index].name, name, MemUnit::NAME_LEN);
    assert(addr < MEMORY_SIZE);
    image->unit[index].offset = addr;
    image->unit[index].size = size;
    image->unit[index].table = table;
    e12[numFile] = _e12;
    memcpy(dataVec + addr, data, size);
    addr += size;
    numFile++;
}


void MemImageUtil::addConfig(uint8_t font, uint8_t bc_r, uint8_t bc_g, uint8_t bc_b, uint8_t ic_r, uint8_t ic_g, int8_t ic_b)
{
    assert(numDir > 0);
    assert(memcmp(image->unit[numDir - 1].name, "config", 6) == 0);
    assert(numFile < MemImage::NUM_FILES);
    int index = MemImage::NUM_DIR + numFile;
    ConfigUnit* config = (ConfigUnit*)&image->unit[index];
    config->soundFont = font;
    
    config->bc_r = bc_r;
    config->bc_g = bc_g;
    config->bc_b = bc_b;

    config->ic_r = ic_r;
    config->ic_g = ic_g;
    config->ic_b = ic_b;

    numFile++;
}



void MemImageUtil::write(const char* name)
{
    assert(addr < MEMORY_SIZE);

    FILE* fp = fopen(name, "wb");
    fwrite(dataVec, addr, 1, fp);
    fclose(fp);
}

void MemImageUtil::writeText(const char* name)
{
    assert(addr < MEMORY_SIZE);
    FILE* fp = fopen(name, "w");

    fprintf(fp, "%d\n", addr);

    static const int STEP = 256;
    char cBuf[STEP*2];

    for (int i = 0; i < addr; i += STEP) {
        int n = STEP;
        if (i + STEP > addr)
            n = addr - i;
        encodeBase64(dataVec + i, n, cBuf, true);
        fprintf(fp, "%s\n", cBuf);
    }
    fclose(fp);
}

void MemImageUtil::dumpConsole()
{
    uint32_t totalSize = 0;
    
    for (int d = 0; d < MemImage::NUM_DIR; ++d) {
        uint32_t dirTotal = 0;

        if (memcmp("config", image->unit[d].name, 6) == 0) {
            printf("config\n");
            for (unsigned f = 0; f < 8; ++f) {
                int index = image->unit[d].offset + f;
                const ConfigUnit* cu = (ConfigUnit*) &image->unit[index];
                printf("  font=%d bc=%02x%02x%02x ic=%02x%02x%02x\n",
                    cu->soundFont,
                    cu->bc_r, cu->bc_g, cu->bc_b,
                    cu->ic_r, cu->ic_g, cu->ic_b);
            }
        } else if (image->unit[d].name[0]) {
            char dirName[9] = { 0 };
            strncpy(dirName, image->unit[d].name, 8);
            printf("Dir: %s\n", dirName);

            for (unsigned f = 0; f < image->unit[d].size; ++f) {
                int index = image->unit[d].offset + f;
                const MemUnit& fileUnit = image->unit[index];
                char fileName[9] = { 0 };
                strncpy(fileName, fileUnit.name, 8);

                printf("   %8s at %8d size=%6d (%3dk) table=%2d ave-err=%7.1f\n",
                    fileName,
                    fileUnit.offset, fileUnit.size, fileUnit.size / 1024,
                    fileUnit.table,
                    sqrtf((float)e12[index - MemImage::NUM_DIR]));

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

    size_t totalImageSize = sizeof(MemImage) + addr;
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
    printf("Directory name hash=%x\n", dirHash);
}


bool MemImageUtil::Test()
{
    MemImageUtil miu;

    TEST((void*)miu.dataVec == (void*)miu.image);

    const uint8_t data4[4] = { 0, 1, 2, 3 };
    const uint8_t data5[5] = { 0, 1, 2, 3, 4 };

    miu.addDir("dir0");
    miu.addFile("file0", data4, 4, 1, 1);
    
    miu.addDir("dir1abcd");
    miu.addFile("file1", data4, 4, 2, 2);
    miu.addFile("file2", data5, 5, 3, 3);

    miu.addDir("config");
    for (int i = 0; i < 8; ++i) {
        miu.addConfig(i + 1, 1, 2, 3, 4, 5, 6);
    }

    TEST(miu.addr == sizeof(MemImage) + 4 + 4 + 5);

    Manifest m;
    // load from memory
    memcpy(m.getBasePtr(), miu.image, sizeof(Manifest));

    TEST(m.getDir("dir2") < 0);
    const MemUnit& muDir0 = m.getUnit(m.getDir("dir0"));
    const MemUnit& muDir1 = m.getUnit(m.getDir("dir1abcd"));

    TEST(m.getFile(m.getDir("dir0"), "file3") < 0);
    const MemUnit& muFile0 = m.getUnit(m.getFile(m.getDir("dir0"), "file0"));
    const MemUnit& muFile1 = m.getUnit(m.getFile(m.getDir("dir1abcd"), "file1"));
    const MemUnit& muFile2 = m.getUnit(m.getFile(m.getDir("dir1abcd"), "file2"));

    // And now check the data
    {
        TEST(muFile0.nameMatch("file0"));
        TEST(muFile0.size == 4);
        TEST(muFile0.table == 1);
        const uint8_t* data = miu.dataVec + muFile0.offset;
        for (int i = 0; i < 4; ++i)
            TEST(data[i] == i);
    }
    {
        TEST(muFile1.size == 4);
    }
    {
        TEST(muFile2.nameMatch("file2"));
        TEST(muFile2.size == 5);
        TEST(muFile2.table == 3);
        const uint8_t* data = miu.dataVec + muFile2.offset;
        for (int i = 0; i < 5; ++i)
            TEST(data[i] == i);
    } 
    // configuration
    int configDir = m.getDir("config");
    const MemUnit& muConfig = m.getUnit(configDir);
    for (int i = 0; i < 8; ++i) {
        const ConfigUnit& c = m.getConfig(muConfig.offset + i);
        TEST(c.soundFont == i + 1);
        TEST(c.bc_r == 1);
        TEST(c.bc_g == 2);
        TEST(c.bc_b == 3);
        TEST(c.ic_r == 4);
        TEST(c.ic_g == 5);
        TEST(c.ic_b == 6);
    }

    return true;
}