#include "PPMalloc/EAGeneralAllocator.h"
#include <xtl.h>
#include <string.h>
#include <stdlib.h>

// Native PPMalloc engine. ARTIST 82B4D730..82B51620 is authoritative;
// the three export holes (D810, E1E8, 50A28) were read from the image.
// Chunk sizes include the eight-byte boundary tag. Free-list links occupy
// user storage, and the next chunk's prior-size word is usable while allocated.
namespace EA { namespace Allocator {
namespace {
    struct PPMMutex
    {
        RTL_CRITICAL_SECTION mCriticalSection;
        int mnLockCount;
    };
    CGS_STATIC_ASSERT(sizeof(PPMMutex) == 32, "ARTIST PPM mutex size");
    CGS_STATIC_ASSERT(offsetof(PPMMutex, mnLockCount) == 28, "ARTIST PPM lock count");

    PPMMutex* MutexCreate(void* pStorage) // 82B4D758
    {
        PPMMutex* pMutex = static_cast<PPMMutex*>(pStorage);
        if (pMutex)
        {
            pMutex->mnLockCount = 0;
            RtlInitializeCriticalSection(&pMutex->mCriticalSection);
        }
        return pMutex;
    }
    int MutexLock(void* pStorage) // 82B4D7A0
    {
        PPMMutex* pMutex = static_cast<PPMMutex*>(pStorage);
        RtlEnterCriticalSection(&pMutex->mCriticalSection);
        return ++pMutex->mnLockCount;
    }
    int MutexUnlock(void* pStorage) // 82B4D7D8
    {
        PPMMutex* pMutex = static_cast<PPMMutex*>(pStorage);
        const int nCount = --pMutex->mnLockCount;
        RtlLeaveCriticalSection(&pMutex->mCriticalSection);
        return nCount;
    }
}

volatile GeneralAllocator::InitCallbackNode* GeneralAllocator::mpInitCallbackNode = 0;

void GeneralAllocator::_AssertLayout()
{
    CGS_STATIC_ASSERT(sizeof(GeneralAllocator) == 0x518, "ARTIST allocator size");
    CGS_STATIC_ASSERT(sizeof(Chunk) == 16, "ARTIST chunk size");
    CGS_STATIC_ASSERT(sizeof(CoreBlock) == 32, "ARTIST core record size");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mpTopChunk) == 0x440, "ARTIST top chunk");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mHeadCoreBlock) == 0x448, "ARTIST core head");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mpMallocFailureFunction) == 0x4ac, "ARTIST failure callback");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mpMutex) == 0x4e4, "ARTIST mutex");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mpMutexData) == 0x4e8, "ARTIST mutex storage");
    CGS_STATIC_ASSERT(offsetof(GeneralAllocator, mNotifyInitState) == 0x514, "ARTIST init notification");
}

GeneralAllocator::PPMAutoMutex::PPMAutoMutex(void* pMutex) : mpMutex(pMutex)
{
    if (mpMutex)
        MutexLock(mpMutex);
}
GeneralAllocator::PPMAutoMutex::~PPMAutoMutex()
{
    if (mpMutex)
        MutexUnlock(mpMutex);
}

void GeneralAllocator::TraceFunctionDefault(const char* pMessage, void*)
{
    OutputDebugStringA(pMessage);
}
void GeneralAllocator::AssertionFailureFunctionDefault(const char* pMessage, void*)
{
    OutputDebugStringA(pMessage);
    __debugbreak();
}

GeneralAllocator::GeneralAllocator(void* pInitialCore, size_t nInitialCoreSize,
    bool bShouldFreeInitialCore, bool bShouldTrim,
    unsigned int (*pCoreFreeFunction)(GeneralAllocator*, void*, unsigned int, void*),
    void* pCoreFreeFunctionContext) // 82B4FF58
{
    mbInitialized = false;
    mnMaxFastBinChunkSize = 0;
    memset(mpFastBinArray, 0, sizeof(mpFastBinArray));
    memset(mpBinArray, 0, sizeof(mpBinArray));
    memset(mBinBitmap, 0, sizeof(mBinBitmap));
    mpTopChunk = 0;
    mpLastRemainderChunk = 0;
    memset(&mHeadCoreBlock, 0, sizeof(mHeadCoreBlock));
    mpHighFence = 0;
    mbHighFenceInternallyDisabled = false;
    mbSystemAllocEnabled = true;
    mnCheckChunkReentrancyCount = 0;
    mcTraceFieldDelimiter = 9;
    mcTraceRecordDelimiter = 10;
    mAutoHeapValidationLevel = kHeapValidationLevelNone;
    mnAutoHeapValidationFrequency = 0;
    mnAutoHeapValidationEventCount = 0;
    mbHeapValidationActive = false;
    mnMMapCount = 0;
    mnMMapMallocTotal = 0;
    mnMMapMaxAllowed = 0;
    mnMMapThreshold = 0;
    mbMMapTopDown = false;
    memset(&mHeadMMapChunk, 0, sizeof(mHeadMMapChunk));
    mpMallocFailureFunction = 0;
    mpMallocFailureFunctionContext = 0;
    mnMaxMallocFailureCount = 256;
    mpAssertionFailureFunction = AssertionFailureFunctionDefault;
    mpAssertionFailureFunctionContext = this;
    mpTraceFunction = TraceFunctionDefault;
    mpTraceFunctionContext = this;
    mnTrimThreshold = 0;
    mnTopPad = 0;
    mpInitialTopChunk = 0;
    mnPageSize = 4096;
    mnNewCoreSize = 0x1000000;
    mnCoreIncrementSize = 0x400000;
    mbTraceInternalMemory = false;
    mpMutex = 0;
    memset(mpMutexData, 0, sizeof(mpMutexData));
    mnFillFree = 0xdd;
    mnFillDelayedFree = 0xde;
    mnFillNew = 0xcd;
    mnFillGuard = 0xab;
    mnFillUnusedCore = 0xfe;
    mpName = 0;
    mNotifyInitState = 0;
    Init(pInitialCore, nInitialCoreSize, bShouldFreeInitialCore, bShouldTrim,
         pCoreFreeFunction, pCoreFreeFunctionContext);
}

bool GeneralAllocator::Init(void* pInitialCore, size_t nInitialCoreSize,
    bool bShouldFreeInitialCore, bool bShouldTrim,
    unsigned int (*pCoreFreeFunction)(GeneralAllocator*, void*, unsigned int, void*),
    void* pCoreFreeFunctionContext) // 82B4FA30
{
    if (!mbInitialized)
    {
        mbInitialized = true;
        SetOption(kOptionEnableThreadSafety, 1);
        PPMAutoMutex lock(mpMutex);
        mnMaxFastBinChunkSize = 64;
        memset(mpFastBinArray, 0, sizeof(mpFastBinArray));
        memset(mpBinArray, 0, sizeof(mpBinArray));
        for (int i = 1; i != 128; ++i)
        {
            Chunk* pBin = GetBin(i);
            pBin->mpPrevChunk = pBin;
            pBin->mpNextChunk = pBin;
        }
        memset(mBinBitmap, 0, sizeof(mBinBitmap));
        mpTopChunk = GetInitialTopChunk();
        mpLastRemainderChunk = 0;
        memset(&mHeadCoreBlock, 0, sizeof(mHeadCoreBlock));
        mHeadCoreBlock.mpPrevCoreBlock = &mHeadCoreBlock;
        mHeadCoreBlock.mpNextCoreBlock = &mHeadCoreBlock;
        mpHighFence = 0;
        mbHighFenceInternallyDisabled = false;
        mnMMapCount = 0;
        mnMMapMallocTotal = 0;
        mnMMapMaxAllowed = 0x10000;
        mnMMapThreshold = 0x20000;
        mnMaxFastBinChunkSize = (mnMaxFastBinChunkSize & 1) | 0x48;
        mbMMapTopDown = false;
        memset(&mHeadMMapChunk, 0, sizeof(mHeadMMapChunk));
        mHeadMMapChunk.mpPrevChunk = &mHeadMMapChunk;
        mHeadMMapChunk.mpNextChunk = &mHeadMMapChunk;
        mnTrimThreshold = 0x40000;
        mnTopPad = 0x10000;
        mpInitialTopChunk = reinterpret_cast<char*>(mpTopChunk);
        mnPageSize = 4096;
    }
    if (pInitialCore || nInitialCoreSize)
        AddCore(pInitialCore, nInitialCoreSize, bShouldFreeInitialCore, bShouldTrim,
                pCoreFreeFunction, pCoreFreeFunctionContext);
    if (!mNotifyInitState)
    {
        mNotifyInitState = 1;
        for (const volatile InitCallbackNode* pNode = mpInitCallbackNode; pNode; pNode = pNode->mpNext)
            pNode->mpInitCallbackFunction(this, true, pNode->mpContext);
    }
    return true;
}

void GeneralAllocator::SetOption(int option, int nValue) // 82B4DF60, table 82148460
{
    switch (option)
    {
    case kOptionEnableThreadSafety:
        if (nValue)
        {
            if (!mpMutex)
                mpMutex = MutexCreate(mpMutexData);
        }
        else if (mpMutex)
        {
            MutexLock(mpMutex);
            void* pMutex = mpMutex;
            mpMutex = 0;
            MutexUnlock(pMutex);
        }
        break;
    case kOptionEnableHighAllocation:
        {
            PPMAutoMutex lock(mpMutex);
            if (nValue)
            {
                if (!mbHighFenceInternallyDisabled)
                {
                    if (!mpHighFence)
                        mpHighFence = reinterpret_cast<void*>(1);
                    AdjustHighFence();
                }
            }
            else
                mpHighFence = 0;
        }
        break;
    case kOptionEnableSystemAlloc: mbSystemAllocEnabled = nValue != 0; break;
    case kOptionTraceInternalMemory:
        mbTraceInternalMemory = nValue != 0; break;
    case kOptionMaxMallocFailureCount: mnMaxMallocFailureCount = nValue; break;
    case kOptionMaxFastBinRequestSize:
        {
            PPMAutoMutex lock(mpMutex);
            ClearFastBins();
            const unsigned int nFlags = mnMaxFastBinChunkSize & 1;
            if (nValue)
            {
                unsigned int nSize = static_cast<unsigned int>(nValue);
                if (nSize > 80) nSize = 80;
                mnMaxFastBinChunkSize = GetChunkSizeFromDataSize(nSize) | nFlags;
            }
            else
                mnMaxFastBinChunkSize = nFlags;
        }
        break;
    case kOptionTrimThreshold: mnTrimThreshold = nValue; break;
    case kOptionTopPad: mnTopPad = nValue; break;
    case kOptionMMapThreshold: mnMMapThreshold = nValue; break;
    case kOptionMMapMaxAllowed: mnMMapMaxAllowed = nValue; break;
    case kOptionMMapTopDown: mbMMapTopDown = nValue != 0; break;
    case kOptionNewCoreSize:
        if (!nValue) mnNewCoreSize = 0x1000000;
        else mnNewCoreSize = AlignUp(static_cast<unsigned int>(nValue) > mnPageSize ? nValue : mnPageSize, mnPageSize);
        break;
    case kOptionCoreIncrementSize:
        if (!nValue) mnCoreIncrementSize = 0x400000;
        else
        {
            mnCoreIncrementSize = AlignUp(static_cast<unsigned int>(nValue) > mnPageSize ? nValue : mnPageSize, mnPageSize);
            if (mnCoreIncrementSize > mnNewCoreSize) mnCoreIncrementSize = mnNewCoreSize;
        }
        break;
    }
}

GeneralAllocator::Chunk* GeneralAllocator::GetBin(int i)
{
    return reinterpret_cast<Chunk*>(reinterpret_cast<char*>(&mpBinArray[2 * i]) - 8);
}
GeneralAllocator::Chunk* GeneralAllocator::GetInitialTopChunk() const
{
    return reinterpret_cast<Chunk*>(const_cast<Chunk**>(mpBinArray));
}
GeneralAllocator::Chunk* GeneralAllocator::GetUnsortedBin() const { return GetInitialTopChunk(); }
unsigned int GeneralAllocator::GetChunkSize(const Chunk* pChunk) { return pChunk->mnSize & 0x7ffffff8; }
GeneralAllocator::Chunk* GeneralAllocator::GetChunkAtOffset(const Chunk* pChunk, int nOffset)
{
    return reinterpret_cast<Chunk*>(reinterpret_cast<char*>(const_cast<Chunk*>(pChunk)) + nOffset);
}
GeneralAllocator::Chunk* GeneralAllocator::GetChunkPtrFromDataPtr(const void* pData)
{
    return reinterpret_cast<Chunk*>(static_cast<char*>(const_cast<void*>(pData)) - 8);
}
void* GeneralAllocator::GetDataPtrFromChunkPtr(const Chunk* pChunk)
{
    return reinterpret_cast<char*>(const_cast<Chunk*>(pChunk)) + 8;
}
unsigned int GeneralAllocator::GetChunkSizeFromDataSize(unsigned int nDataSize)
{
    const unsigned int nSize = nDataSize + 11;
    return nSize > 16 ? nSize & ~7u : 16;
}
size_t GeneralAllocator::AlignUp(size_t nValue, size_t nAlignment)
{
    return (nValue + nAlignment - 1) & ~(nAlignment - 1);
}
void GeneralAllocator::UnlinkChunk(const Chunk* pChunk)
{
    pChunk->mpPrevChunk->mpNextChunk = pChunk->mpNextChunk;
    pChunk->mpNextChunk->mpPrevChunk = pChunk->mpPrevChunk;
}
void GeneralAllocator::LinkChunk(Chunk* pChunk, Chunk* pChunkPrev, Chunk* pChunkNext)
{
    pChunk->mpPrevChunk = pChunkPrev;
    pChunk->mpNextChunk = pChunkNext;
    pChunkNext->mpPrevChunk = pChunk;
    pChunkPrev->mpNextChunk = pChunk;
}
int GeneralAllocator::GetFastBinChunksExist() const { return mnMaxFastBinChunkSize & 1; }
int GeneralAllocator::GetLargeBinIndexFromChunkSize(unsigned int nSize) // 82B4D810, static
{
    if ((nSize >> 6) <= 32) return 56 + (nSize >> 6);
    if ((nSize >> 9) <= 20) return 91 + (nSize >> 9);
    if ((nSize >> 12) <= 10) return 110 + (nSize >> 12);
    if ((nSize >> 15) <= 4) return 119 + (nSize >> 15);
    if ((nSize >> 18) <= 2) return 124 + (nSize >> 18);
    return 126;
}
void GeneralAllocator::AdjustHighFence()
{
    if (mpHighFence && mpTopChunk)
        mpHighFence = reinterpret_cast<char*>(mpTopChunk) + ((mpTopChunk->mnSize >> 1) & 0x3ffffffc);
}
void GeneralAllocator::SetNewTopChunk(Chunk* pChunk, bool bChunkIsNew) // 82B4D948
{
    Chunk* pOldTop = mpTopChunk;
    mpTopChunk = pChunk;
    pChunk->mpNextChunk = pChunk;
    mpTopChunk->mpPrevChunk = mpTopChunk->mpNextChunk;
    if (pOldTop != GetInitialTopChunk() && bChunkIsNew)
        LinkChunk(pOldTop, GetUnsortedBin(), GetUnsortedBin()->mpNextChunk);
    if (!mbHighFenceInternallyDisabled)
        AdjustHighFence();
}
void GeneralAllocator::LinkCoreBlock(CoreBlock* pCoreBlock, CoreBlock* pCoreBlockPrev)
{
    pCoreBlock->mpNextCoreBlock = pCoreBlockPrev;
    pCoreBlock->mpPrevCoreBlock = pCoreBlockPrev->mpPrevCoreBlock;
    pCoreBlockPrev->mpPrevCoreBlock = pCoreBlock;
    pCoreBlock->mpPrevCoreBlock->mpNextCoreBlock = pCoreBlock;
}
void GeneralAllocator::UnlinkCoreBlock(CoreBlock* pCoreBlock) // 82B4DE08
{
    pCoreBlock->mpPrevCoreBlock->mpNextCoreBlock = pCoreBlock->mpNextCoreBlock;
    pCoreBlock->mpNextCoreBlock->mpPrevCoreBlock = pCoreBlock->mpPrevCoreBlock;
}
GeneralAllocator::Chunk* GeneralAllocator::MakeChunkFromCore(void* pCore, size_t nCoreSize, int nFlags)
{
    Chunk* pChunk = reinterpret_cast<Chunk*>(pCore);
    pChunk->mnPriorSize = 0;
    pChunk->mnSize = nCoreSize | nFlags;
    const unsigned int nSize = ((GetChunkSize(pChunk) - 9) & ~7u);
    pChunk->mnSize = (pChunk->mnSize & 0x80000007) | nSize;
    Chunk* pFence = GetChunkAtOffset(pChunk, nSize);
    pFence->mnPriorSize = nSize;
    pFence->mnSize = 8;
    Chunk* pSecondFence = GetChunkAtOffset(pFence, 8);
    pSecondFence->mnPriorSize = 8;
    pSecondFence->mnSize = 9;
    return pChunk;
}
bool GeneralAllocator::AddCore(void* pCore, size_t nSize, bool bShouldFree, bool bShouldTrim,
    unsigned int (*pCoreFreeFunction)(GeneralAllocator*, void*, unsigned int, void*), void* pContext) // 82B4F800
{
    PPMAutoMutex lock(mpMutex);
    Chunk* pChunk;
    bool bNewTop;
    if (pCore)
    {
        if (nSize < 64) return false;
        if (nSize % mnPageSize) nSize &= ~(mnPageSize - 1);
        if (nSize & 15) nSize &= ~15u;
        CoreBlock* pBlock = static_cast<CoreBlock*>(pCore);
        pBlock->mnSize = nSize;
        pBlock->mnReservedSize = nSize;
        pBlock->mbShouldFree = bShouldFree;
        pBlock->mbShouldFreeOnShutdown = bShouldFree;
        pBlock->mbShouldTrim = bShouldTrim;
        pBlock->mpCoreFreeFunction = pCoreFreeFunction;
        pBlock->mpCore = reinterpret_cast<char*>(AlignUp(reinterpret_cast<size_t>(pBlock) + sizeof(CoreBlock), 8));
        pBlock->mbMMappedMemory = false;
        pBlock->mpCoreFreeFunctionContext = pContext;
        LinkCoreBlock(pBlock, &mHeadCoreBlock);
        pChunk = MakeChunkFromCore(pBlock->mpCore, reinterpret_cast<char*>(pBlock) + nSize - pBlock->mpCore, 1);
        bNewTop = mpTopChunk != GetInitialTopChunk();
    }
    else
    {
        if (!nSize) return false;
        pChunk = AddCoreInternal(AlignUp(nSize, mnPageSize));
        if (!pChunk) return false;
        bNewTop = true;
    }
    SetNewTopChunk(pChunk, bNewTop);
    return true;
}

void* GeneralAllocator::MMapAllocInternal(size_t nSize, size_t* pSize) // 82B4DE98
{
    void* pCore = VirtualAlloc(0, nSize, mbMMapTopDown ? 0x103000 : 0x3000, PAGE_READWRITE);
    if (pCore && pSize) *pSize = nSize;
    return pCore;
}
GeneralAllocator::Chunk* GeneralAllocator::AddCoreInternal(size_t nMinSize) // 82B4EF08
{
    int nFlags = 1;
    const size_t nMinimum = AlignUp(nMinSize, mnPageSize);
    size_t nSize = nMinimum;
    if (nSize < mnNewCoreSize) nSize = AlignUp(mnNewCoreSize, mnPageSize);
    const size_t nReserved = nSize;
    void* pCore = malloc(nSize); // Original XDK CRT core acquisition, not the allocation engine.
    if (!pCore)
    {
        for (;;)
        {
            nSize = (3 * nSize) >> 2;
            if (nSize < nMinimum) break;
            pCore = malloc(nSize);
            if (pCore) break;
        }
        if (!pCore)
        {
            nSize = nReserved;
            pCore = MMapAllocInternal(nReserved, &nSize);
            if (!pCore) return 0;
            nFlags = 3;
        }
    }
    CoreBlock* pBlock = static_cast<CoreBlock*>(pCore);
    pBlock->mnSize = nSize;
    pBlock->mnReservedSize = nSize;
    pBlock->mbMMappedMemory = (nFlags & 2) != 0;
    pBlock->mpCore = reinterpret_cast<char*>(AlignUp(reinterpret_cast<size_t>(pBlock) + sizeof(CoreBlock), 8));
    pBlock->mbShouldFreeOnShutdown = true;
    pBlock->mpCoreFreeFunction = 0;
    pBlock->mpCoreFreeFunctionContext = 0;
    pBlock->mbShouldFree = mHeadCoreBlock.mpNextCoreBlock != &mHeadCoreBlock;
    pBlock->mbShouldTrim = pBlock->mbShouldFree;
    LinkCoreBlock(pBlock, &mHeadCoreBlock);
    if (mpHighFence && mHeadCoreBlock.mpNextCoreBlock != mHeadCoreBlock.mpPrevCoreBlock)
    {
        mpHighFence = 0;
        mbHighFenceInternallyDisabled = true;
    }
    return MakeChunkFromCore(pBlock->mpCore, reinterpret_cast<char*>(pBlock) + nSize - pBlock->mpCore, nFlags);
}
GeneralAllocator::Chunk* GeneralAllocator::ExtendCoreInternal(size_t nMinSize) // 82B4F9A8
{
    Chunk* pChunk = AddCoreInternal(nMinSize);
    if (pChunk)
    {
        SetNewTopChunk(pChunk, true);
        const unsigned int nSize = GetChunkSize(pChunk);
        if (nSize > nMinSize)
        {
            Chunk* pRemainder = GetChunkAtOffset(pChunk, nMinSize);
            const unsigned int nRemainderSize = nSize - nMinSize;
            pChunk->mnSize = nMinSize | 1;
            pRemainder->mnPriorSize = nMinSize;
            pRemainder->mnSize = nRemainderSize;
            GetChunkAtOffset(pRemainder, nRemainderSize)->mnPriorSize = nRemainderSize;
            SetNewTopChunk(pRemainder, false);
        }
    }
    return pChunk;
}
bool GeneralAllocator::FreeCore(CoreBlock* pCoreBlock, bool bShouldFreeCoreBlock) // 82B4DE28
{
    if (pCoreBlock->mbShouldFree || (bShouldFreeCoreBlock && pCoreBlock->mbShouldFreeOnShutdown))
    {
        if (pCoreBlock->mpCoreFreeFunction)
            pCoreBlock->mpCoreFreeFunction(this, pCoreBlock, pCoreBlock->mnSize, pCoreBlock->mpCoreFreeFunctionContext);
        else
            free(pCoreBlock);
        return true;
    }
    return false;
}
void* GeneralAllocator::MMapMalloc(size_t nSize) // 82B4F1E8
{
    const size_t nRequest = AlignUp(nSize + 31, mnPageSize);
    size_t nCoreSize = nRequest;
    if (nRequest < nSize) return 0;
    Chunk* pChunk = static_cast<Chunk*>(MMapAllocInternal(nRequest, &nCoreSize));
    if (!pChunk) return 0;
    ++mnMMapCount;
    mnMMapMallocTotal += nCoreSize;
    unsigned int nChunkSize = nCoreSize - 16;
    unsigned int nOffset = 0;
    if (reinterpret_cast<size_t>(pChunk) & 7)
    {
        nOffset = 8 - (reinterpret_cast<size_t>(pChunk) & 7);
        nChunkSize -= nOffset;
        pChunk = GetChunkAtOffset(pChunk, nOffset);
        pChunk->mnSize = (nChunkSize - nOffset) | 2;
    }
    else pChunk->mnSize = nChunkSize | 2;
    pChunk->mnPriorSize = nOffset;
    Chunk* pList = GetChunkAtOffset(pChunk, nChunkSize);
    pList->mnPriorSize = nChunkSize;
    pList->mnSize = 19;
    LinkChunk(pList, &mHeadMMapChunk, mHeadMMapChunk.mpNextChunk);
    return GetDataPtrFromChunkPtr(pChunk);
}

GeneralAllocator::CoreBlock* GeneralAllocator::FindCoreBlockForAddress(const void* pAddress) const
{
    for (CoreBlock* pBlock = mHeadCoreBlock.mpPrevCoreBlock; pBlock != &mHeadCoreBlock; pBlock = pBlock->mpPrevCoreBlock)
        if (reinterpret_cast<size_t>(pAddress) >= reinterpret_cast<size_t>(pBlock) &&
            reinterpret_cast<size_t>(pAddress) < reinterpret_cast<size_t>(pBlock) + pBlock->mnSize)
            return pBlock;
    return 0;
}
bool GeneralAllocator::GetChunkIsFenceChunk(const Chunk* pChunk) const // 82B4E1E8
{
    if ((pChunk->mnSize & 0x7ffffff8) >= 16) return false;
    const CoreBlock* pBlock = FindCoreBlockForAddress(pChunk);
    return pBlock && reinterpret_cast<size_t>(pChunk) >= reinterpret_cast<size_t>(pBlock) + pBlock->mnSize - 16;
}
GeneralAllocator::Chunk* GeneralAllocator::FindAndSetNewTopChunk() // 82B4D878
{
    Chunk* pBest = GetInitialTopChunk();
    unsigned int nLargest = 0;
    for (CoreBlock* pBlock = mHeadCoreBlock.mpPrevCoreBlock; pBlock != &mHeadCoreBlock; pBlock = pBlock->mpPrevCoreBlock)
    {
        Chunk* pFence = reinterpret_cast<Chunk*>(reinterpret_cast<char*>(pBlock) + pBlock->mnSize - 16);
        for (Chunk* pChunk = reinterpret_cast<Chunk*>(pBlock->mpCore); pChunk < pFence; )
        {
            unsigned int nSize = GetChunkSize(pChunk);
            Chunk* pNext = GetChunkAtOffset(pChunk, nSize);
            if (!(pNext->mnSize & 1))
            {
                if (nSize > 0x2000 || pNext == pFence)
                {
                    pBest = pChunk;
                    goto found;
                }
                if (nSize > nLargest) { pBest = pChunk; nLargest = nSize; }
            }
            pChunk = pNext;
        }
    }
found:
    if (pBest != GetInitialTopChunk())
    {
        UnlinkChunk(pBest);
        pBest->mpNextChunk = pBest;
        pBest->mpPrevChunk = pBest;
    }
    mpTopChunk = pBest;
    return pBest;
}
size_t GeneralAllocator::TrimCore(size_t) // 82B4F0A0: padding argument unused on X360
{
    PPMAutoMutex lock(mpMutex);
    size_t nFreed = 0;
    CoreBlock* pBlock = mHeadCoreBlock.mpPrevCoreBlock;
    while (pBlock != &mHeadCoreBlock)
    {
        if (pBlock->mbShouldFree)
        {
            Chunk* pFence = reinterpret_cast<Chunk*>(reinterpret_cast<char*>(pBlock) + pBlock->mnSize - 16);
            if (!(pFence->mnSize & 1))
            {
                Chunk* pChunk = GetChunkAtOffset(pFence, -static_cast<int>(pFence->mnPriorSize));
                if (reinterpret_cast<char*>(pChunk) < pBlock->mpCore + 16)
                {
                    Chunk* pPrev = pChunk->mpPrevChunk;
                    Chunk* pNext = pChunk->mpNextChunk;
                    CoreBlock* pOldBlock = pBlock;
                    UnlinkChunk(pChunk);
                    pBlock = pBlock->mpNextCoreBlock;
                    UnlinkCoreBlock(pOldBlock);
                    const unsigned int nSize = pOldBlock->mnSize;
                    if (FreeCore(pOldBlock, false))
                    {
                        nFreed += nSize;
                        if (pChunk == mpTopChunk)
                        {
                            mpTopChunk = GetInitialTopChunk();
                            FindAndSetNewTopChunk();
                        }
                    }
                    else
                    {
                        LinkChunk(pChunk, pPrev, pNext);
                        LinkCoreBlock(pOldBlock, pBlock);
                        pBlock = pOldBlock;
                    }
                }
            }
        }
        pBlock = pBlock->mpPrevCoreBlock;
    }
    return nFreed;
}

void GeneralAllocator::FreeInternal(void* pData) // 82B4F5D8
{
    if (!pData) return;
    Chunk* pChunk = GetChunkPtrFromDataPtr(pData);
    unsigned int nSize = GetChunkSize(pChunk);
    if (nSize <= mnMaxFastBinChunkSize && (!mpHighFence || pChunk < mpHighFence))
    {
        const unsigned int nIndex = (nSize >> 3) - 2;
        mnMaxFastBinChunkSize |= 1;
        pChunk->mpNextChunk = mpFastBinArray[nIndex];
        pChunk->mnSize |= 0x80000004;
        mpFastBinArray[nIndex] = pChunk;
        return;
    }
    Chunk* pNext = GetChunkAtOffset(pChunk, nSize);
    if (pChunk->mnSize & 2)
    {
        void* pCore = reinterpret_cast<char*>(pChunk) - pChunk->mnPriorSize;
        unsigned int nCoreSize = pChunk->mnPriorSize + nSize;
        UnlinkChunk(pNext);
        --mnMMapCount;
        mnMMapMallocTotal -= nCoreSize + 16;
        VirtualFree(pCore, 0, MEM_RELEASE);
        return;
    }
    const unsigned int nNextSize = GetChunkSize(pNext);
    if (!(pChunk->mnSize & 1))
    {
        nSize += pChunk->mnPriorSize;
        pChunk = GetChunkAtOffset(pChunk, -static_cast<int>(pChunk->mnPriorSize));
        pChunk->mnSize = nSize | 1;
        pNext->mnPriorSize = nSize;
        UnlinkChunk(pChunk);
    }
    if (GetChunkAtOffset(pNext, nNextSize)->mnSize & 1)
    {
        pNext->mnPriorSize = nSize;
        pNext->mnSize &= ~1u;
    }
    else
    {
        nSize += nNextSize;
        UnlinkChunk(pNext);
        pChunk->mnSize = nSize | 1;
        GetChunkAtOffset(pChunk, nSize)->mnPriorSize = nSize;
    }
    if (pChunk == mpTopChunk || pNext == mpTopChunk)
    {
        mpTopChunk = pChunk;
        pChunk->mpNextChunk = pChunk;
        pChunk->mpPrevChunk = pChunk;
        pChunk->mnSize = nSize | 1;
        GetChunkAtOffset(pChunk, nSize)->mnPriorSize = nSize;
        AdjustHighFence();
    }
    else
        LinkChunk(pChunk, GetUnsortedBin(), GetUnsortedBin()->mpNextChunk);
    if (nSize >= 0x10000 && GetChunkIsFenceChunk(GetChunkAtOffset(pChunk, nSize)) && nSize >= mnTrimThreshold)
        TrimCore(mnTopPad);
}
void GeneralAllocator::Free(void* pData) // 82B4FC08
{
    PPMAutoMutex lock(mpMutex);
    FreeInternal(pData);
}
void GeneralAllocator::ClearFastBins() // 82B4FC70
{
    if (!mnMaxFastBinChunkSize)
    {
        Init(0, 0, true, false, 0, 0);
        return;
    }
    const unsigned int nLast = (mnMaxFastBinChunkSize >> 3) - 2;
    for (unsigned int i = 0; i <= nLast; ++i)
    {
        Chunk* pChunk = mpFastBinArray[i];
        if (!pChunk) continue;
        mpFastBinArray[i] = 0;
        do
        {
            Chunk* pNextFast = pChunk->mpNextChunk;
            pChunk->mnSize &= 0x7ffffffb;
            unsigned int nSize = GetChunkSize(pChunk);
            Chunk* pNext = GetChunkAtOffset(pChunk, nSize);
            unsigned int nNextSize = GetChunkSize(pNext);
            if (!(pChunk->mnSize & 1))
            {
                nSize += pChunk->mnPriorSize;
                pChunk = GetChunkAtOffset(pChunk, -static_cast<int>(pChunk->mnPriorSize));
                pChunk->mnSize = nSize | 1;
                pNext->mnPriorSize = nSize;
                UnlinkChunk(pChunk);
            }
            if (GetChunkAtOffset(pNext, nNextSize)->mnSize & 1)
            {
                pNext->mnPriorSize = nSize;
                pNext->mnSize &= ~1u;
            }
            else
            {
                nSize += nNextSize;
                UnlinkChunk(pNext);
                pChunk->mnSize = nSize | 1;
                GetChunkAtOffset(pChunk, nSize)->mnPriorSize = nSize;
            }
            if (pChunk == mpTopChunk || pNext == mpTopChunk)
            {
                mpTopChunk = pChunk;
                pChunk->mpNextChunk = pChunk;
                pChunk->mpPrevChunk = pChunk;
                pChunk->mnSize = nSize | 1;
                GetChunkAtOffset(pChunk, nSize)->mnPriorSize = nSize;
                AdjustHighFence();
            }
            else
                LinkChunk(pChunk, GetUnsortedBin(), GetUnsortedBin()->mpNextChunk);
            pChunk = pNextFast;
        } while (pChunk);
    }
    mnMaxFastBinChunkSize &= ~1u;
}

bool GeneralAllocator::Shutdown() // 82B500F8
{
    if (mNotifyInitState == 1)
    {
        mNotifyInitState = 0;
        for (const volatile InitCallbackNode* pNode = mpInitCallbackNode; pNode; pNode = pNode->mpNext)
            pNode->mpInitCallbackFunction(this, false, pNode->mpContext);
    }
    if (mpMutex) MutexLock(mpMutex);
    if (mbInitialized)
    {
        mbInitialized = false;
        if (GetFastBinChunksExist()) ClearFastBins();
        while (mHeadMMapChunk.mpNextChunk != &mHeadMMapChunk)
        {
            Chunk* pList = mHeadMMapChunk.mpNextChunk;
            FreeInternal(GetDataPtrFromChunkPtr(GetChunkAtOffset(pList, -static_cast<int>(pList->mnPriorSize))));
        }
        while (mHeadCoreBlock.mpNextCoreBlock != &mHeadCoreBlock)
        {
            CoreBlock* pCore = mHeadCoreBlock.mpNextCoreBlock;
            UnlinkCoreBlock(pCore);
            FreeCore(pCore, true);
        }
        mpTopChunk = GetInitialTopChunk();
        mpHighFence = 0;
        mbHighFenceInternallyDisabled = false;
    }
    if (mpMutex)
    {
        void* pMutex = mpMutex;
        mpMutex = 0;
        MutexUnlock(pMutex);
        // RtlDeleteCriticalSection is a no-op in the actual XDK.
        RtlDeleteCriticalSection(&static_cast<PPMMutex*>(pMutex)->mCriticalSection);
    }
    return true;
}
GeneralAllocator::~GeneralAllocator() { Shutdown(); }

void* GeneralAllocator::MallocInternal(size_t nSize, int nAllocationFlags) // 82B502C0
{
    unsigned int nFlags = nAllocationFlags;
    unsigned int nFailureCount = 0;
    const unsigned int nPaddedSize = nSize + 11;
    unsigned int nChunkSize;
    int nBinIndex;
    bool bHigh;
    Chunk* pChunk;
    Chunk* pBin;
    Chunk* pUnsorted = GetUnsortedBin();
    unsigned int nSizeAvailable;
    unsigned int nRemainderSize;
    bool bSetLastRemainder;

    if (nFlags & 8)
        return MMapMalloc(nSize);
retry:
    nChunkSize = nPaddedSize > 16 ? nPaddedSize & ~7u : 16;
    if (nChunkSize <= mnMaxFastBinChunkSize)
    {
        const unsigned int nFastIndex = (nChunkSize >> 3) - 2;
        pChunk = mpFastBinArray[nFastIndex];
        if (pChunk)
        {
            mpFastBinArray[nFastIndex] = pChunk->mpNextChunk;
            pChunk->mnSize &= 0x7ffffffb;
            return GetDataPtrFromChunkPtr(pChunk);
        }
    }
    bHigh = (nFlags & 1) != 0;
    if (nChunkSize < 0x200)
    {
        nBinIndex = nChunkSize >> 3;
        pBin = GetBin(nBinIndex);
        pChunk = pBin->mpPrevChunk;
        if (pChunk != pBin && (!mpHighFence ||
            (bHigh ? reinterpret_cast<size_t>(pChunk) + nChunkSize >= reinterpret_cast<size_t>(mpHighFence)
                   : reinterpret_cast<size_t>(pChunk) < reinterpret_cast<size_t>(mpHighFence))))
        {
            GetChunkAtOffset(pChunk, nChunkSize)->mnSize |= 1;
            UnlinkChunk(pChunk);
            return GetDataPtrFromChunkPtr(pChunk);
        }
    }
    else
    {
        nBinIndex = GetLargeBinIndexFromChunkSize(nChunkSize);
        if (GetFastBinChunksExist()) ClearFastBins();
    }
scan_unsorted:
    while ((pChunk = pUnsorted->mpPrevChunk) != pUnsorted)
    {
        nSizeAvailable = GetChunkSize(pChunk);
        Chunk* pPrev = pChunk->mpPrevChunk;
        const bool bMatches = !mpHighFence ||
            (bHigh ? reinterpret_cast<size_t>(pChunk) + nSizeAvailable >= reinterpret_cast<size_t>(mpHighFence)
                   : reinterpret_cast<size_t>(pChunk) < reinterpret_cast<size_t>(mpHighFence));
        if (nChunkSize < 0x200 && pChunk == mpLastRemainderChunk && pPrev == pUnsorted &&
            nSizeAvailable > nChunkSize + 16 && bMatches)
        {
            Chunk* pRemainder = GetChunkAtOffset(pChunk, nChunkSize);
            const unsigned int nRemaining = nSizeAvailable - nChunkSize;
            pUnsorted->mpNextChunk = pRemainder;
            pUnsorted->mpPrevChunk = pRemainder;
            mpLastRemainderChunk = pRemainder;
            pRemainder->mpNextChunk = pUnsorted;
            pRemainder->mpPrevChunk = pUnsorted;
            pChunk->mnSize = nChunkSize | 1;
            pRemainder->mnSize = nRemaining | 1;
            GetChunkAtOffset(pRemainder, nRemaining)->mnPriorSize = nRemaining;
            return GetDataPtrFromChunkPtr(pChunk);
        }
        pUnsorted->mpPrevChunk = pPrev;
        pPrev->mpNextChunk = pUnsorted;
        if (nSizeAvailable == nChunkSize && bMatches)
        {
            GetChunkAtOffset(pChunk, nSizeAvailable)->mnSize |= 1;
            return GetDataPtrFromChunkPtr(pChunk);
        }
        const int nIndex = nSizeAvailable < 0x200 ? nSizeAvailable >> 3 : GetLargeBinIndexFromChunkSize(nSizeAvailable);
        Chunk* pBefore = GetBin(nIndex);
        Chunk* pAfter = pBefore->mpNextChunk;
        if (nSizeAvailable >= 0x200 && pBefore != pAfter)
        {
            if (nSizeAvailable >= pBefore->mpPrevChunk->mnSize)
            {
                while (nSizeAvailable < GetChunkSize(pAfter)) pAfter = pAfter->mpNextChunk;
                pBefore = pAfter->mpPrevChunk;
            }
            else
            {
                pAfter = pBefore;
                pBefore = pBefore->mpPrevChunk;
            }
        }
        mBinBitmap[nIndex >> 5] |= 1u << (nIndex & 31);
        LinkChunk(pChunk, pBefore, pAfter);
    }
    if (nChunkSize >= 0x200)
    {
        pBin = GetBin(nBinIndex);
        for (pChunk = pBin->mpPrevChunk; pChunk != pBin; pChunk = pChunk->mpPrevChunk)
        {
            nSizeAvailable = GetChunkSize(pChunk);
            if (nSizeAvailable >= nChunkSize && (!mpHighFence ||
                (bHigh ? reinterpret_cast<size_t>(pChunk) + nSizeAvailable >= reinterpret_cast<size_t>(mpHighFence)
                       : reinterpret_cast<size_t>(pChunk) < reinterpret_cast<size_t>(mpHighFence))))
            {
                bSetLastRemainder = false;
                goto use_chunk;
            }
        }
    }
    {
        unsigned int nWordIndex = (nBinIndex + 1) >> 5;
        unsigned int nBit = 1u << ((nBinIndex + 1) & 31);
        unsigned int nMap = mBinBitmap[nWordIndex];
        pBin = GetBin(nBinIndex + 1);
        for (;;)
        {
            if (nBit > nMap || !nBit)
            {
                do
                {
                    if (++nWordIndex >= 4) goto use_top;
                    nMap = mBinBitmap[nWordIndex];
                } while (!nMap);
                nBit = 1;
                pBin = GetBin(nWordIndex << 5);
            }
            while (!(nBit & nMap))
            {
                nBit <<= 1;
                pBin = GetChunkAtOffset(pBin, 8);
            }
            pChunk = pBin->mpPrevChunk;
            if (pChunk == pBin)
            {
                nMap &= ~nBit;
                mBinBitmap[nWordIndex] = nMap;
                pBin = GetChunkAtOffset(pBin, 8);
                nBit <<= 1;
                continue;
            }
            nSizeAvailable = GetChunkSize(pChunk);
            if (mpHighFence)
            {
                do
                {
                    if (bHigh ? reinterpret_cast<size_t>(pChunk) + nSizeAvailable >= reinterpret_cast<size_t>(mpHighFence)
                              : reinterpret_cast<size_t>(pChunk) < reinterpret_cast<size_t>(mpHighFence)) break;
                    pChunk = pChunk->mpPrevChunk;
                    nSizeAvailable = GetChunkSize(pChunk);
                } while (pChunk != pBin);
                if (pChunk == pBin)
                {
                    pBin = GetChunkAtOffset(pBin, 8);
                    nBit <<= 1;
                    continue;
                }
            }
            bSetLastRemainder = nChunkSize < 0x200;
            goto use_chunk;
        }
    }
use_top:
    pChunk = mpTopChunk;
    nSizeAvailable = GetChunkSize(pChunk);
    if (nSizeAvailable >= nChunkSize + 16)
    {
        Chunk* pNewTop;
        if (mpHighFence && bHigh)
        {
            pNewTop = pChunk;
            pChunk = GetChunkAtOffset(pChunk, nSizeAvailable - nChunkSize);
            if (reinterpret_cast<size_t>(pChunk) & 7)
            {
                pChunk = reinterpret_cast<Chunk*>(reinterpret_cast<size_t>(pChunk) & ~7u);
                nRemainderSize = reinterpret_cast<char*>(pChunk) - reinterpret_cast<char*>(pNewTop);
                nChunkSize = nSizeAvailable - nRemainderSize;
            }
            else nRemainderSize = nSizeAvailable - nChunkSize;
            pChunk->mnSize = nChunkSize;
            Chunk* pNext = GetChunkAtOffset(pChunk, nChunkSize);
            pNext->mnPriorSize = nChunkSize;
            pNext->mnSize |= 1;
        }
        else
        {
            nRemainderSize = nSizeAvailable - nChunkSize;
            pNewTop = GetChunkAtOffset(pChunk, nChunkSize);
            pChunk->mnSize = nChunkSize | 1;
            pNewTop->mnSize = nRemainderSize | 1;
        }
        mpTopChunk = pNewTop;
        pNewTop->mpNextChunk = pNewTop;
        pNewTop->mpPrevChunk = pNewTop;
        pNewTop->mnSize = nRemainderSize | 1;
        GetChunkAtOffset(pNewTop, nRemainderSize)->mnPriorSize = nRemainderSize;
        AdjustHighFence();
        return GetDataPtrFromChunkPtr(pChunk);
    }
    if (GetFastBinChunksExist())
    {
        ClearFastBins();
        nBinIndex = nChunkSize >> 3;
        goto scan_unsorted;
    }
    if (mbSystemAllocEnabled && nChunkSize >= mnMMapThreshold && mnMMapCount < mnMMapMaxAllowed)
    {
        void* pData = MMapMalloc(nSize);
        if (pData) return pData;
    }
    if (mpHighFence && !(nFlags & 0x40000000))
    {
        nFlags |= 0x40000000;
        if (bHigh) nFlags &= ~3u;
        else nFlags |= 1;
        nFailureCount = 0;
        goto retry;
    }
    if (mbSystemAllocEnabled)
    {
        pChunk = ExtendCoreInternal(nChunkSize);
        if (pChunk)
        {
            GetChunkAtOffset(pChunk, GetChunkSize(pChunk))->mnSize |= 1;
            return GetDataPtrFromChunkPtr(pChunk);
        }
    }
    if (mpMallocFailureFunction && ++nFailureCount < mnMaxMallocFailureCount)
    {
        // ARTIST releases every recursive acquisition before calling user code.
        // The callback path requires the native mutex, as does the original.
        int nLockCount = static_cast<PPMMutex*>(mpMutex)->mnLockCount;
        for (int i = 0; i < nLockCount; ++i) MutexUnlock(mpMutex);
        const bool bRetry = mpMallocFailureFunction(this, nSize, nSize + 64, mpMallocFailureFunctionContext);
        for (int i = 0; i < nLockCount; ++i) MutexLock(mpMutex);
        if (bRetry) goto retry;
    }
    return 0;
use_chunk:
    nRemainderSize = nSizeAvailable - nChunkSize;
    UnlinkChunk(pChunk);
    if (nRemainderSize < 16)
        GetChunkAtOffset(pChunk, nSizeAvailable)->mnSize |= 1;
    else
    {
        Chunk* pRemainder = GetChunkAtOffset(pChunk, nChunkSize);
        pUnsorted->mpNextChunk = pRemainder;
        pUnsorted->mpPrevChunk = pRemainder;
        pRemainder->mpNextChunk = pUnsorted;
        pRemainder->mpPrevChunk = pUnsorted;
        if (bSetLastRemainder) mpLastRemainderChunk = pRemainder;
        pChunk->mnSize = nChunkSize | 1;
        pRemainder->mnSize = nRemainderSize | 1;
        GetChunkAtOffset(pRemainder, nRemainderSize)->mnPriorSize = nRemainderSize;
    }
    return GetDataPtrFromChunkPtr(pChunk);
}

void* GeneralAllocator::MallocAlignedInternal(size_t nSize, size_t nAlignment,
    size_t nAlignmentOffset, int nAllocationFlags) // 82B50A28, raw image
{
    if (nAlignment <= 8 && !nAlignmentOffset)
        return MallocInternal(nSize, nAllocationFlags);
    if (nAlignment < 16) nAlignment = 16;
    if (nAlignment & (nAlignment - 1))
    {
        unsigned int nMask = nAlignment - 1;
        nMask |= nMask >> 1;
        nMask |= nMask >> 2;
        nMask |= nMask >> 4;
        nMask |= nMask >> 8;
        nMask |= nMask >> 16;
        nAlignment = nMask + 1;
    }
    if (nAlignmentOffset & 7) nAlignmentOffset = (nAlignmentOffset + 7) & ~7u;
    const unsigned int nChunkSize = GetChunkSizeFromDataSize(nSize);
    if (nAllocationFlags & 4)
    {
        Chunk* pHead = GetUnsortedBin();
        for (int nPass = 0; nPass != 2; ++nPass)
        {
            for (Chunk* pChunk = pHead->mpNextChunk; pChunk != pHead; pChunk = pChunk->mpNextChunk)
                if (GetChunkSize(pChunk) == nChunkSize && (!nAlignment ||
                    !((reinterpret_cast<size_t>(pChunk) + nAlignmentOffset + 8) & (nAlignment - 1))))
                {
                    GetChunkAtOffset(pChunk, nChunkSize)->mnSize |= 1;
                    UnlinkChunk(pChunk);
                    return GetDataPtrFromChunkPtr(pChunk);
                }
            const int nIndex = nChunkSize < 0x200 ? nChunkSize >> 3 : GetLargeBinIndexFromChunkSize(nChunkSize);
            // The original second scan starts at this stored pointer, rather
            // than the usual GetBin(nIndex) sentinel (82B50B58..82B50B64).
            pHead = mpBinArray[nIndex];
        }
    }
    void* pData = MallocInternal(nChunkSize + nAlignment + nAlignmentOffset + 16, nAllocationFlags);
    if (!pData) return 0;
    Chunk* pChunk = GetChunkPtrFromDataPtr(pData);
    if ((reinterpret_cast<size_t>(pData) + nAlignmentOffset) % nAlignment)
    {
        Chunk* pAligned = reinterpret_cast<Chunk*>(((reinterpret_cast<size_t>(pData) + nAlignment +
            nAlignmentOffset - 1) & -nAlignment) - nAlignmentOffset - 8);
        if (reinterpret_cast<char*>(pAligned) - reinterpret_cast<char*>(pChunk) < 16)
            pAligned = GetChunkAtOffset(pAligned, nAlignment);
        const unsigned int nPrefix = reinterpret_cast<char*>(pAligned) - reinterpret_cast<char*>(pChunk);
        const unsigned int nRemaining = GetChunkSize(pChunk) - nPrefix;
        if (pChunk->mnSize & 2)
        {
            pAligned->mnSize = nRemaining | 2;
            pAligned->mnPriorSize = pChunk->mnPriorSize + nPrefix;
            GetChunkAtOffset(pAligned, nRemaining)->mnPriorSize = nRemaining;
            return GetDataPtrFromChunkPtr(pAligned);
        }
        pAligned->mnSize = nRemaining | 1;
        Chunk* pNext = GetChunkAtOffset(pAligned, nRemaining);
        pNext->mnPriorSize = nRemaining;
        pNext->mnSize |= 1;
        pChunk->mnSize = (pChunk->mnSize & 0x80000007) | nPrefix;
        pAligned->mnPriorSize = nPrefix;
        HeapValidationLevel nSavedLevel = mAutoHeapValidationLevel;
        mAutoHeapValidationLevel = kHeapValidationLevelNone;
        FreeInternal(GetDataPtrFromChunkPtr(pChunk));
        mAutoHeapValidationLevel = nSavedLevel;
        pChunk = pAligned;
    }
    if (!(pChunk->mnSize & 2) && GetChunkSize(pChunk) > nChunkSize + 16)
    {
        unsigned int nRemaining = GetChunkSize(pChunk) - nChunkSize;
        Chunk* pRemainder = GetChunkAtOffset(pChunk, nChunkSize);
        Chunk* pNext = GetChunkAtOffset(pRemainder, nRemaining);
        pRemainder->mnSize = nRemaining | 1;
        pChunk->mnSize = (pChunk->mnSize & 0x80000007) | nChunkSize;
        const unsigned int nNextSize = GetChunkSize(pNext);
        if (GetChunkAtOffset(pNext, nNextSize)->mnSize & 1)
        {
            HeapValidationLevel nSavedLevel = mAutoHeapValidationLevel;
            mAutoHeapValidationLevel = kHeapValidationLevelNone;
            FreeInternal(GetDataPtrFromChunkPtr(pRemainder));
            mAutoHeapValidationLevel = nSavedLevel;
        }
        else
        {
            UnlinkChunk(pNext);
            nRemaining += nNextSize;
            pRemainder->mnSize = nRemaining | 1;
            GetChunkAtOffset(pRemainder, nRemaining)->mnPriorSize = nRemaining;
            if (pRemainder == mpTopChunk || pNext == mpTopChunk)
            {
                mpTopChunk = pRemainder;
                pRemainder->mpNextChunk = pRemainder;
                pRemainder->mpPrevChunk = pRemainder;
                pRemainder->mnSize = nRemaining | 1;
                GetChunkAtOffset(pRemainder, nRemaining)->mnPriorSize = nRemaining;
                AdjustHighFence();
            }
            else LinkChunk(pRemainder, GetUnsortedBin(), GetUnsortedBin()->mpNextChunk);
        }
    }
    return GetDataPtrFromChunkPtr(pChunk);
}

void* GeneralAllocator::MallocAligned(size_t nSize, size_t nAlignment,
    size_t nAlignmentOffset, int nAllocationFlags) // 82B515B0
{
    PPMAutoMutex lock(mpMutex);
    return MallocAlignedInternal(nSize, nAlignment, nAlignmentOffset, nAllocationFlags);
}

} }
