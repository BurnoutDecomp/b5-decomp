#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <intrin.h>

struct AptFile
{
    volatile long mnRefCount = 1;
    int mnDeletes = 0;
};

// The allocator and final file deletion are observed boundaries. Assignment,
// disposal, vector growth, swap and destruction are extracted production bodies.
static unsigned guChecks = 0;
static unsigned guFailures = 0;
static void Check(bool lbPass, const char* lpcMessage)
{
    ++guChecks;
    if (!lbPass)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcMessage);
    }
}

struct Pool
{
    std::unordered_map<void*, size_t> mBlocks;

    void* Allocate(size_t luSize)
    {
        void* lpBlock = std::malloc(luSize);
        mBlocks.emplace(lpBlock, luSize);
        return lpBlock;
    }

    void Deallocate(void* lpBlock, size_t luSize)
    {
        const auto liBlock = mBlocks.find(lpBlock);
        Check(liBlock != mBlocks.end(), "only an owned heap block is freed");
        if (liBlock == mBlocks.end())
            return;
        Check(liBlock->second == luSize, "heap release retains its allocation size");
        std::free(lpBlock);
        mBlocks.erase(liBlock);
    }

    ~Pool()
    {
        for (const auto& lrBlock : mBlocks)
            std::free(lrBlock.first);
    }
};

static Pool gPool;
static Pool* gpAptSharedPtrPool = &gPool;
static int AptSharedPtrDecRef(AptFile* lpFile)
{
    return _InterlockedDecrement(&lpFile->mnRefCount);
}
static int AptSharedPtrDelete(AptFile* lpFile)
{
    ++lpFile->mnDeletes;
    return 0;
}

#include "apt_file_lifetime_types.inc"
#include "apt_file_lifetime_bodies.inc"

using Pending = AptLinker::PendingFileVector;

static void Initialise(Pending& lrFiles)
{
    lrFiles.mnSize = 0;
    lrFiles.mnCapacity = 0;
    lrFiles.mpData = lrFiles.mInlineStorage;
    lrFiles.mInlineStorage[0].pData = nullptr;
    lrFiles.mInlineStorage[1].pData = nullptr;
}

static void Append(Pending& lrFiles, AptFile* lpFiles, int liCount)
{
    for (int liFile = 0; liFile < liCount; ++liFile)
    {
        AptFilePtr lFile;
        lFile.pData = &lpFiles[liFile];  // borrowed producer reference
        lrFiles.PushBack(lFile);
    }
}

static void Growth(int liCount)
{
    AptFile laFiles[9];
    {
        Pending lPending;
        Initialise(lPending);
        Append(lPending, laFiles, liCount);
        Check(lPending.size() == liCount, "growth preserves pending count");
        for (int liFile = 0; liFile < liCount; ++liFile)
        {
            Check(lPending.mpData[liFile].pData == &laFiles[liFile],
                  "growth preserves file order");
            Check(laFiles[liFile].mnRefCount == 2,
                  "growth owns exactly one reference per file");
        }
    }
    for (int liFile = 0; liFile < liCount; ++liFile)
    {
        Check(laFiles[liFile].mnRefCount == 1, "destruction returns the vector reference");
        AptSharedPtr<AptFile>::Dispose(&laFiles[liFile]);
        Check(laFiles[liFile].mnRefCount == 0 && laFiles[liFile].mnDeletes == 1,
              "the last owner releases the file and bundle pin exactly once");
    }
    Check(gPool.mBlocks.empty(), "growth and destruction release every heap block");
}

static void Swap(int liFirstCount, int liSecondCount)
{
    AptFile laFirst[5], laSecond[5];
    {
        Pending lFirst, lSecond;
        Initialise(lFirst);
        Initialise(lSecond);
        Append(lFirst, laFirst, liFirstCount);
        Append(lSecond, laSecond, liSecondCount);
        lFirst.Swap(lSecond);
        Check(lFirst.size() == liSecondCount && lSecond.size() == liFirstCount,
              "swap exchanges both logical sizes");
        Check(lFirst.mpData != lSecond.mInlineStorage &&
              lSecond.mpData != lFirst.mInlineStorage,
              "swap never borrows the other vector's inline storage");
        for (int liFile = 0; liFile < liSecondCount; ++liFile)
            Check(lFirst.mpData[liFile].pData == &laSecond[liFile],
                  "swap retains the second vector's files");
        for (int liFile = 0; liFile < liFirstCount; ++liFile)
            Check(lSecond.mpData[liFile].pData == &laFirst[liFile],
                  "swap retains the first vector's files");
    }
    for (int liFile = 0; liFile < liFirstCount; ++liFile)
        Check(laFirst[liFile].mnRefCount == 1, "swap releases each first-side reference");
    for (int liFile = 0; liFile < liSecondCount; ++liFile)
        Check(laSecond[liFile].mnRefCount == 1, "swap releases each second-side reference");
    Check(gPool.mBlocks.empty(), "swap destruction releases both heap allocations");
}

int main()
{
    for (int liCount : {0, 1, 2, 3, 5, 9})
        Growth(liCount);
#ifndef APT_LIFETIME_GROWTH_ONLY
    Swap(0, 0);
    Swap(1, 2);
    Swap(3, 0);  // Update's populated.Swap(empty) path
    Swap(0, 3);
    Swap(3, 5);
#endif
    std::printf("AptFileLifetime: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
