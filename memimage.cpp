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
    image = (MemImage*)dataVec;
    memset(dataVec, 0, MEMORY_SIZE);
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


void MemImageUtil::writePalette(int index, const MemPalette& palette)
{
    assert(index >= 0 && index < MemPalette::NUM_PALETTES);
    memcpy(dataVec + Manifest::PaletteAddr(index), &palette, sizeof(palette));
}


void MemImageUtil::writeDesc(const char* desc)
{
    size_t len = strlen(desc);
    if (len > MemImage::SIZE_DESC - 1)
        len = MemImage::SIZE_DESC - 1;
    memset(dataVec + Manifest::DescAddr(), 0, MemImage::SIZE_DESC);
    for (size_t i = 0; i < len; ++i)
        *(dataVec + Manifest::DescAddr() + i) = desc[i];
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

        if (image->unit[d].name[0]) {
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

    MemPalette palette[MemPalette::NUM_PALETTES];
    memcpy(palette, dataVec + Manifest::PaletteAddr(0), sizeof(MemPalette) * MemPalette::NUM_PALETTES);

    for (int i = 0; i < MemPalette::NUM_PALETTES; ++i) {
        printf("  %d font=%d bc=%02x%02x%02x ic=%02x%02x%02x\n",
            i,
            palette[i].soundFont,
            palette[i].bladeColor.r, palette[i].bladeColor.g, palette[i].bladeColor.b,
            palette[i].impactColor.r, palette[i].impactColor.g, palette[i].impactColor.b);
    }
    printf("Description=%s\n", (const char*)(dataVec + Manifest::DescAddr()));

    size_t totalImageSize = sizeof(MemImage) + addr;
    printf("Image size=%d bytes, %d k\n", int(totalImageSize), int(totalImageSize / 1024));
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

    TEST(miu.addr == MemImage::SIZE_BASE + 4 + 4 + 5);

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
    return true;
}