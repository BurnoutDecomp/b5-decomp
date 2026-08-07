#include "SDKs/EATech/Apt/DogmaAllocator.h"

#include <cstring>   // memset
#include <cstdlib>   // (the retired CRT-heap leaf backing; kept for the debug/guard paths)
#include <intrin.h>  // _Interlocked* (MSVC)

// TEMPORARY DEBUG: guard-page allocator for the DOGMA pool -- every Allocate gets
// its own virtual page(s), every Deallocate flips the block PAGE_NOACCESS and never
// reuses it, so a use-after-free faults AT THE WRITER instead of corrupting the
// free lists. Enable only for a diagnosis boot; must be 0 for committed builds.
#define APT_DOGMA_GUARD 0
#if APT_DOGMA_GUARD
#include <windows.h>
// Deferred-revoke ring: pages queue here on Deallocate and get PAGE_NOACCESS'd a
// few Allocate calls later (the GC free path touches its MemItem header right
// after the pool free; immediate revocation would fault the freeing code itself).
namespace
{
    void*    s_apDogmaGuardPending[64];
    unsigned s_nDogmaGuardHead;
    unsigned s_nDogmaGuardCount;
}
void DogmaGuard_DrainRevokes(bool bForce)
{
    // Keep the newest 32 frees un-revoked (a grace window); revoke the rest.
    const unsigned nKeep = bForce ? 0u : 32u;
    while (s_nDogmaGuardCount > nKeep)
    {
        void* p = s_apDogmaGuardPending[(s_nDogmaGuardHead - s_nDogmaGuardCount) & 63u];
        DWORD nOld;
        ::VirtualProtect(reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(p) & ~static_cast<uintptr_t>(0xFFF)),
            0x1000, PAGE_NOACCESS, &nOld);
        --s_nDogmaGuardCount;
    }
}
void DogmaGuard_QueueRevoke(void* p)
{
    if (s_nDogmaGuardCount == 64u)
        DogmaGuard_DrainRevokes(false);
    s_apDogmaGuardPending[s_nDogmaGuardHead & 63u] = p;
    ++s_nDogmaGuardHead;
    ++s_nDogmaGuardCount;
}
#endif

// The host user-function table the heap-leaf hook slots live in (Apt.h; defined
// in CgsAptAux.cpp -- the console dword_8324E818 object).
#include "SDKs/EATech/include/Apt/Apt.h"
extern AptUserFunctions gAptFuncs;   // dword_8324E818 (CgsAptAux.cpp)

// ---------------------------------------------------------------------------
// DOGMA heap leaf primitives -- the bottom of the DOGMA pool allocator.
//
// On X360 these are the three host-heap hook slots at dword_8324E818 /
// dword_8324E81C / dword_8324E820 -- the FIRST THREE members of the gAptFuncs
// host user-function table (Apt.h: pfnMemAlloc +0x00 / pfnMemFree +0x04 /
// pfnMemFreeSize +0x08; dword_8324E818 == &gAptFuncs), installed by
// CgsGui::AptAux::ConstructApt @0x5BA0F8 with CgsGui::AptCallbackMemory::Alloc
// @0x828491C8 / Free @0x828492A8 / FreeSize @0x82849358 (the AptAux data-handler
// heap). The dispatch is a single indirect call through the slot -- exactly how
// AptAllocatorInitialize @0x82ADD118 calls dword_8324E818(48) for the
// pool-manager objects. Every DOGMA pool reach runs inside AptAux::Prepare ->
// InitializeApt, AFTER ConstructApt installed the hooks and
// mAptDataHandler.Prepare wired the allocator (CgsAptAux.cpp); the one static
// DOGMA object, gAptValueGCPool(0,0), early-outs before allocating, so the
// through-the-table call is live at every reach.
// ---------------------------------------------------------------------------
void* DOGMA_Malloc(size_t nSize)
{
    return gAptFuncs.pfnMemAlloc(nSize);     // X360 dword_8324E818(size) == gAptFuncs.pfnMemAlloc
}

void DOGMA_Free(void* pBlock)
{
    gAptFuncs.pfnMemFree(pBlock);            // X360 dword_8324E81C(ptr) == gAptFuncs.pfnMemFree
}

void DOGMA_FreeSized(void* pBlock, size_t nSize)
{
    gAptFuncs.pfnMemFreeSize(pBlock, nSize); // X360 dword_8324E820(ptr, size) == gAptFuncs.pfnMemFreeSize
}

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

    mnOffsetToStoreNext = nOffsetToStoreNextInFreeItem >> 2;                   // a1[4] = v30 >> 2
    mnOffsetToStoreSize = nOffsetToStoreSizeInFreeItem >> 2;                   // a1[5] = a8 >> 2

    // PC static-init accommodation (boot-verified): the X360 never constructs a DOGMA pool with a
    // 0-byte main size -- gAptValueGCPool is HEAP-allocated at AptInit (AptAllocatorInitialize
    // @0x82ADD118), AFTER StaticInitialize() sets the item sizes, with real pool sizes. On PC
    // gAptValueGCPool is a static object constructed (0,0) before StaticInitialize runs, so
    // maxSizeAllocation/mainPoolSizeBytes are 0 here. A 0-size DOGMA_Malloc + SetupPool would
    // write the pool header into a 0-byte block (heap corruption). Defer the allocation: leave
    // the pool EMPTY until it is (re)initialized at AptInit with real sizes.
    if (mainPoolSizeBytes == 0)
    {
        mpaFirstFreeBySize = 0;
        mpFirstPool        = 0;
        return;
    }

    // Per-size free-list head array: one head per 4-byte size bucket up to the
    // max allocation, plus the zero bucket -- ((maxSize>>2)+1) buckets. (x64
    // widening, Phase-0 regime): each bucket is a uintptr_t* (the free-list head), so the array is
    // sizeof(uintptr_t*) per bucket -- NOT the console literal 4. Allocating 4*N
    // here left buckets 33..64 (for maxSize 256) past the block, read as garbage
    // free-list pointers by Allocate -> returned to the caller -> access violation.
    const size_t nFreeListBytes = sizeof(uintptr_t*) * ((maxSizeAllocation >> 2) + 1);
    mpaFirstFreeBySize = (uintptr_t**)DOGMA_Malloc(nFreeListBytes);

    // First (main) pool.
    DOGMA_MemPool* pFirstPool = (DOGMA_MemPool*)DOGMA_Malloc(mainPoolSizeBytes);
    mpFirstPool = pFirstPool;

    memset(mpaFirstFreeBySize, 0, sizeof(uintptr_t*) * ((mnMaxSizeAllocation >> 2) + 1));

    pFirstPool->SetupPool(0, mainPoolSizeBytes);   // *v36=0; v36[1]=v36[2]=a2-15
}

// ---------------------------------------------------------------------------
// Destructor @ 0x82ADB950
// ---------------------------------------------------------------------------
DOGMA_PoolManager::~DOGMA_PoolManager()
{
    // Free the per-size free-list head array (same byte count it was allocated --
    // sizeof(uintptr_t*) per bucket on x64; see the ctor).
    DOGMA_FreeSized(mpaFirstFreeBySize, sizeof(uintptr_t*) * ((mnMaxSizeAllocation >> 2) + 1));

    // Free every pool in the chain. (while, not do/while: an empty/deferred pool has
    // mpFirstPool == 0 -- see the 0-size guard in the ctor -- and must not be dereffed.)
    DOGMA_MemPool* pPool = mpFirstPool;
    while (pPool)
    {
        DOGMA_MemPool* pNext = pPool->GetNextPool();
        DOGMA_Free(pPool);
        pPool = pNext;
    }

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
#if APT_DOGMA_GUARD
    // TEMPORARY DEBUG (see the flag above): one fresh committed region per block,
    // the payload flushed against the END of the last page so overruns fault too.
    {
        extern void DogmaGuard_DrainRevokes(bool bForce);
        DogmaGuard_DrainRevokes(false);
        const size_t nGuardSize = (nAllocatedSize + 0xFFFu) & ~static_cast<size_t>(0xFFFu);
        unsigned char* pRegion = static_cast<unsigned char*>(
            ::VirtualAlloc(nullptr, nGuardSize + 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        DWORD nOld;
        ::VirtualProtect(pRegion + nGuardSize, 0x1000, PAGE_NOACCESS, &nOld);   // tail guard page
        const size_t nOffset = (nGuardSize - nAllocatedSize) & ~static_cast<size_t>(15);
        return pRegion + nOffset;
    }
#endif
    // Round the request up, then clamp to the minimum. (x64 alignment fix, Phase-0 regime): the
    // console rounds to a 4-BYTE multiple (v3 = (v3&~3)+4) -- fine for its 32-bit
    // pointers, but the pool CARVES blocks sequentially (ConsumeBytes), so a block
    // whose size is 4-mod-8 (e.g. a 12/20-byte request) leaves every SUBSEQUENT block
    // at a 4-mod-8 address. On x64 a vtbl'd AptValue there is MISALIGNED: its 8-byte
    // members straddle an 8-byte word and the tail (e.g. EAStringC::m_pData's high
    // dword) shares a word with adjacent free-filled memory -> 0xBAADF00D corruption
    // (the EAStringC use-after-free in Add2). Round to 8 so every carved block stays
    // 8-aligned. Deallocate mirrors this so the per-size free-list buckets still match.
    size_t nSize = nAllocatedSize;                          // v3
    if (nSize < mnMinimumAllocationSize)
        nSize = mnMinimumAllocationSize;
    nSize = (nSize + 7) & ~static_cast<size_t>(7);

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
        //    push it onto the outside-allocation list. (x64 widening, Phase-0 regime): the node
        //    header is pNext+pPrev = 2*sizeof(ptr) bytes (== GetStructOverHead(), 16 on
        //    x64), and GetReturnedPointer() returns base+16; the console literal `+8`
        //    under-allocated by 8 -> the payload overran the block. Use the real overhead.
        OutsideAllocationT* pNode =
            (OutsideAllocationT*)DOGMA_Malloc(nAllocatedSize + OutsideAllocationT::GetStructOverHead());
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
#if APT_DOGMA_GUARD
    // TEMPORARY DEBUG (see the flag above): queue the page for revocation a few
    // allocations from now (the GC-value free path clears its MemItem flag right
    // AFTER this call, so an immediate revoke would fault on the freeing code
    // itself); any later touch of the freed block faults at the offender.
    {
        extern void DogmaGuard_QueueRevoke(void* p);
        DogmaGuard_QueueRevoke(pNowFree);
        return true;
    }
#endif
    // x64 alignment fix (Phase-0 regime -- MUST mirror Allocate so the per-size free-list
    // buckets match): round to an 8-byte multiple, not the console's 4-byte one.
    size_t nSize = nAllocatedSize;                          // v5
    if (nSize < mnMinimumAllocationSize)
        nSize = mnMinimumAllocationSize;
    nSize = (nSize + 7) & ~static_cast<size_t>(7);

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
            nFreeSize = nAllocatedSize + OutsideAllocationT::GetStructOverHead();   // console a3+8 == payload + 2*sizeof(ptr); x64 node overhead is 16 -- must mirror Allocate's widened node

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
