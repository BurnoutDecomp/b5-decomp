#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <cstring>   // memset
#include <intrin.h>  // _Interlocked* (MSVC)

// ===========================================================================
// DOGMA_PoolManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// SHAPE is the Feb-2007 leak's DogmaAllocator.h; BODIES are the X360 pseudocode.
// The free-list mutations in Allocate/Deallocate are bracketed by interrupt-
// masking spinlocks (one per free-list family); the pool-carve walk uses the
// _DOGMA_MemPool accessors (CanFitBytes / GetNextPool / ConsumeBytes / SetupPool)
// rather than raw pointer indexing.
// ===========================================================================

// The three DOGMA free-list spinlocks (X360 globals unk_8324E758 / unk_8324E8EC
// / unk_8324E720). One guards the per-size free lists, one the overflow pool
// list, one the outside-allocation list.
static DOGMA_SpinLock gDogmaPoolFreeListLock;     // unk_8324E758
static DOGMA_SpinLock gDogmaPoolCarveLock;        // unk_8324E8EC
static DOGMA_SpinLock gDogmaOutsideAllocLock;     // unk_8324E720

void DOGMA_SpinLock::Lock()
{
    // X360: interrupt-masking lwarx/stwcx. test-and-set. Modelled portably as an
    // atomic compare-exchange spin (0 -> 1 acquires).
    while (_InterlockedCompareExchange(&mlHeld, 1, 0) != 0)
    {
    }
}

void DOGMA_SpinLock::Unlock()
{
    _InterlockedExchange(&mlHeld, 0);
}

// ---------------------------------------------------------------------------
// operator new / delete @ scalar deleting destructor (0x82ADD1D8) backing.
//
// The X360 `scalar deleting destructor' runs ~DOGMA_PoolManager() then, when the
// delete flag is set, frees the object via dword_8324E820(this, 48) -- 48 ==
// sizeof on X360. MSVC synthesizes that thunk itself from operator delete, so it
// is not hand-written; these route the allocation through the same DOGMA heap.
// ---------------------------------------------------------------------------
void* DOGMA_PoolManager::operator new(size_t size)
{
    return DOGMA_Malloc(size);
}

void DOGMA_PoolManager::operator delete(void* p, size_t size)
{
    DOGMA_FreeSized(p, size);
}

void* DOGMA_PoolManager::operator new[](size_t size)
{
    return DOGMA_Malloc(size);
}

void DOGMA_PoolManager::operator delete[](void* p)
{
    DOGMA_Free(p);
}

// ---------------------------------------------------------------------------
// Constructor @ 0x82ADB850
//
// Param order matches the leak (DogmaAllocator.h:315). mnOverflowPoolSize and
// mnMaxSizeAllocation are const members set ONLY by the initializer list; the
// X360 ctor body writes the remaining members and builds the first pool.
// ---------------------------------------------------------------------------
DOGMA_PoolManager::DOGMA_PoolManager(size_t mainPoolSizeBytes,
    size_t overflowPoolSizeBytes,
    size_t minSizeAllocation,
    size_t maxSizeAllocation,
    uint8_t nOffsetToStoreNextInFreeItem,
    bool bStoreFreeBlockSize, uint8_t nOffsetToStoreSizeInFreeItem,
    bool bTrackOusideAllocations)
    : mnOverflowPoolSize(overflowPoolSizeBytes)   // a1[2] = a3
    , mnMaxSizeAllocation(maxSizeAllocation)      // a1[3] = a5
{
    mbTrackOutsideAllocations = bTrackOusideAllocations;   // a1[8] = a28

    // Smallest item must hold the in-free-item bookkeeping (next ptr, and the
    // optional size word): minimum = max(nOffsetToStoreNext + 4,
    // bStoreFreeBlockSize ? nOffsetToStoreSize + 4 : 0).
    size_t nMinimumItemSize = (size_t)nOffsetToStoreNextInFreeItem + 4;        // v32 = a6 + 4
    if (bStoreFreeBlockSize && (size_t)nOffsetToStoreSizeInFreeItem + 4 > nMinimumItemSize)
        nMinimumItemSize = (size_t)nOffsetToStoreSizeInFreeItem + 4;           // v32 = a8 + 4

    mpaFirstFreeBySize = 0;       // *a1   = 0  (re-assigned below)
    mpFirstPool = 0;              // a1[1] = 0  (re-assigned below)
    mnMinimumAllocationSize = (uint32_t)minSizeAllocation;                     // a1[6] = a4
    mpFirstOutSideAllocation = 0; // a1[9]  = 0
    mnItemsAllocated = 0;         // a1[10] = 0
    mnItemsFreed = 0;             // a1[11] = 0
    mbStoreFreeBlockSize = bStoreFreeBlockSize;                                // a1[7] = a7

    if (minSizeAllocation < nMinimumItemSize)
        mnMinimumAllocationSize = (uint32_t)nMinimumItemSize;                  // a1[6] = v32

    // Per-size free-list head array: one head per 4-byte size bucket up to the
    // max allocation, plus the zero bucket. (4 * ((maxSize >> 2) + 1) bytes.)
    mpaFirstFreeBySize = (uintptr_t**)DOGMA_Malloc(4 * ((maxSizeAllocation >> 2) + 1));

    // First (main) pool.
    DOGMA_MemPool* pFirstPool = (DOGMA_MemPool*)DOGMA_Malloc(mainPoolSizeBytes);
    mpFirstPool = pFirstPool;

    memset(mpaFirstFreeBySize, 0, 4 * ((mnMaxSizeAllocation >> 2) + 1));

    mnOffsetToStoreNext = nOffsetToStoreNextInFreeItem >> 2;                   // a1[4] = v30 >> 2
    mnOffsetToStoreSize = nOffsetToStoreSizeInFreeItem >> 2;                   // a1[5] = a8 >> 2

    pFirstPool->SetupPool(0, mainPoolSizeBytes);   // *v36=0; v36[1]=v36[2]=a2-15
}

// ---------------------------------------------------------------------------
// Destructor @ 0x82ADB950
// ---------------------------------------------------------------------------
DOGMA_PoolManager::~DOGMA_PoolManager()
{
    // Free the per-size free-list head array (same byte count it was allocated).
    DOGMA_FreeSized(mpaFirstFreeBySize, 4 * ((mnMaxSizeAllocation >> 2) + 1));

    // Free every pool in the chain.
    DOGMA_MemPool* pPool = mpFirstPool;
    do
    {
        DOGMA_MemPool* pNext = pPool->GetNextPool();
        DOGMA_Free(pPool);
        pPool = pNext;
    }
    while (pPool);

    // Free every tracked outside allocation.
    if (mbTrackOutsideAllocations)
    {
        OutsideAllocationT* pOutside = mpFirstOutSideAllocation;
        if (pOutside)
        {
            do
            {
                OutsideAllocationT* pNext = pOutside->pNext;
                DOGMA_Free(pOutside);
                pOutside = pNext;
            }
            while (pOutside);
        }
    }
}

// ---------------------------------------------------------------------------
// Allocate @ 0x82AE1058
// ---------------------------------------------------------------------------
void* DOGMA_PoolManager::Allocate(size_t nAllocatedSize)
{
    // Round the request up to a 4-byte multiple, then clamp to the minimum.
    size_t nSize = nAllocatedSize;                          // v3
    if ((nAllocatedSize & 3) != 0)
        nSize = (nAllocatedSize & 0xFFFFFFFC) + 4;
    if (nSize < mnMinimumAllocationSize)
        nSize = mnMinimumAllocationSize;

    size_t nMax = mnMaxSizeAllocation;                      // v4
    ++mnItemsAllocated;

    if (nSize <= nMax)
    {
        // -- In-pool path: pull from the per-size free list, else carve a pool.
        void* pResult;

        gDogmaPoolFreeListLock.Lock();
        {
            uintptr_t** paBySize = mpaFirstFreeBySize;                 // v25 = *a1
            uintptr_t* pFree = paBySize[(nSize & 0xFFFFFFFC) >> 2];    // v26 = *(((v3&~3)) + *a1)
            if (pFree)
            {
                pResult = pFree;
                // Unlink: head = *(free + mnOffsetToStoreNext).
                paBySize[(nSize & 0xFFFFFFFC) >> 2] = (uintptr_t*)pFree[mnOffsetToStoreNext];
                --mnItemsFreed;
            }
            else
            {
                pResult = 0;
            }
        }
        gDogmaPoolFreeListLock.Unlock();

        if (!pResult)
        {
            // No free item of this size: carve `nSize` bytes from the first pool
            // that can fit it, allocating a fresh overflow pool if none can.
            gDogmaPoolCarveLock.Lock();
            {
                DOGMA_MemPool* pPool = mpFirstPool;                     // v35 = a1[1]
                while (!pPool->CanFitBytes(nSize))                      // v35[2] < v3
                {
                    pPool = pPool->GetNextPool();                      // v35 = *v35
                    if (!pPool)
                    {
                        // Out of room: allocate and link a new overflow pool.
                        DOGMA_MemPool* pNewPool =
                            (DOGMA_MemPool*)DOGMA_Malloc(mnOverflowPoolSize);   // v36
                        pNewPool->SetupPool(mpFirstPool, mnOverflowPoolSize);
                        mpFirstPool = pNewPool;
                        pResult = pNewPool->ConsumeBytes(nSize);
                        gDogmaPoolCarveLock.Unlock();
                        return pResult;
                    }
                }
                pResult = pPool->ConsumeBytes(nSize);                  // v35+v42-v41+12
            }
            gDogmaPoolCarveLock.Unlock();
        }

        return pResult;
    }
    else if (mbTrackOutsideAllocations)
    {
        // -- Tracked outside allocation: wrap with an intrusive list node and
        //    push it onto the outside-allocation list.
        OutsideAllocationT* pNode =
            (OutsideAllocationT*)DOGMA_Malloc(nAllocatedSize + 8);     // v6 = malloc(a2+8)
        pNode->pPrev = 0;                                              // v6[1] = 0

        gDogmaOutsideAllocLock.Lock();
        {
            OutsideAllocationT* pHead = mpFirstOutSideAllocation;      // v14 = a1[9]
            pNode->pNext = pHead;                                      // *v6 = v14
            if (pHead)
                pHead->pPrev = pNode;                                  // *(v14+4) = v6
            mpFirstOutSideAllocation = pNode;                          // a1[9] = v6
        }
        gDogmaOutsideAllocLock.Unlock();

        return pNode->GetReturnedPointer();                           // v6 + 2
    }
    else
    {
        // -- Untracked oversize: straight through to the heap.
        return DOGMA_Malloc(nAllocatedSize);
    }
}

// ---------------------------------------------------------------------------
// Deallocate @ 0x82AE1308
// ---------------------------------------------------------------------------
bool DOGMA_PoolManager::Deallocate(void* pNowFree, size_t nAllocatedSize)
{
    size_t nSize = nAllocatedSize;                          // v5
    if ((nAllocatedSize & 3) != 0)
        nSize = (nAllocatedSize & 0xFFFFFFFC) + 4;
    if (nSize < mnMinimumAllocationSize)
        nSize = mnMinimumAllocationSize;

    size_t nMax = mnMaxSizeAllocation;                      // v6
    --mnItemsAllocated;

    if (nSize <= nMax)
    {
        // -- In-pool path: push the freed item back onto its per-size free list.
        uintptr_t* pFreed = (uintptr_t*)pNowFree;           // a2

        gDogmaPoolFreeListLock.Lock();
        {
            uintptr_t** paBySize = mpaFirstFreeBySize;                 // v27 = *a1
            uint32_t nNextOffset = mnOffsetToStoreNext;                // v28 = a1[4]
            ++mnItemsFreed;
            // *(free + mnOffsetToStoreNext) = current head.
            pFreed[nNextOffset] = (uintptr_t)paBySize[(nSize & 0xFFFFFFFC) >> 2];
            if (mbStoreFreeBlockSize)
                pFreed[mnOffsetToStoreSize] = nSize;                   // a2[a1[5]] = v5
            paBySize[(nSize & 0xFFFFFFFC) >> 2] = pFreed;              // head = a2
        }
        gDogmaPoolFreeListLock.Unlock();

        return true;
    }
    else
    {
        // -- Outside allocation path.
        size_t nFreeSize;                                   // v7
        if (mbTrackOutsideAllocations)
        {
            nFreeSize = nAllocatedSize + 8;

            // Recover the list node sitting in front of the returned pointer.
            OutsideAllocationT* pNode =
                (OutsideAllocationT*)(((uintptr_t*)pNowFree) - 2);     // a2 -= 2

            gDogmaOutsideAllocLock.Lock();
            {
                if (pNode->pNext)
                    pNode->pNext->pPrev = pNode->pPrev;               // *(*a2+4) = a2[1]
                OutsideAllocationT* pPrev = pNode->pPrev;             // v15 = a2[1]
                if (pPrev)
                    pPrev->pNext = pNode->pNext;                      // *v15 = *a2
                if (mpFirstOutSideAllocation == pNode)
                    mpFirstOutSideAllocation = pNode->pNext;          // a1[9] = *a2
            }
            gDogmaOutsideAllocLock.Unlock();

            pNowFree = pNode;
        }
        else
        {
            nFreeSize = nAllocatedSize;
        }

        DOGMA_FreeSized(pNowFree, nFreeSize);               // dword_8324E820(a2, v7)
        return false;
    }
}
