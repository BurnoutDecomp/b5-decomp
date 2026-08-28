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

// ---------------------------------------------------------------------------
// ⭐ THE FREE-ITEM SLOT WIDTH -- the console `4` that has to be an `8` on x64.
//
// A DOGMA free item stores its bookkeeping (the free-list next link, and
// optionally the block size) in slots of one machine word. The console word is
// 32-bit, so its ctor scales a BYTE offset into a slot index with `>> 2`, sizes
// the per-size bucket array `4 * ((max >> 2) + 1)`, and indexes it `size >> 2` --
// three separate spellings of the same literal 4.
//
// The x64 arbiter (Burnout_External_Xbox_One, rung 1 for Apt) replaces ALL THREE
// with 8. The out-of-line ctor sub_140827C60 is unambiguous:
//     v11 = a6 + 8;  if ( a7 && a8 + 8 > v11 ) v11 = a8 + 8;
//     *a1 = alloc(8 * (a5 >> 3) + 8);   memset(v14, 0, 8 * (max >> 3) + 8);
//     *(a1 + 32) = a6 >> 3;             *(a1 + 36) = a8 >> 3;
// and Allocate (sub_14082D590) / Deallocate (inlined at sub_140828040) index the
// slots as `*(pItem + 8 * mnOffsetToStoreNext)` / `*(pItem + 8 * mnOffsetToStoreSize)`
// and the buckets as `*a1 + 8 * (size >> 3)`.
//
// ⛔ WHY THIS MATTERED (measured 2026-08-28): only the bucket ELEMENT width had been
// widened; the two `>> 2` shifts survived. With gAptValueGCSizeOffset == 8 that made
// mnOffsetToStoreSize == 2, so Deallocate wrote the freed block's size at byte +16
// while the GC pool walk (AptValueGCPoolManager.cpp ItemSizeWord, and the x64 walk
// sub_140838090: `v5 += *(v5 + 8) & ~1`) reads it at byte +8. Byte +8 still held the
// dead AptValue's bitfield, whose top 7 bits are meValueType -- so the "size" the walk
// stepped by was >= 0x02000000 and the very first freed block threw the cursor clean
// out of the pool. The walk agreed with mnItemsAllocated exactly until the first free
// and never again: 617/617 at pass 3, 618 vs 766 at pass 4, 632 vs 2767 by pass 10.
// ~77% of live AptValues were never marked, never swept, never PreDestroy'd, so
// AptCIH::PreDestroy never fired pfnOnUnload and the AptCommunicator's 256-entry
// component table overflowed on the second Driver Details entry.
// ---------------------------------------------------------------------------
static const size_t KN_FREE_SLOT_BYTES = sizeof(uintptr_t);   // x64: 8 (console: 4)
static const uint32_t KN_FREE_SLOT_SHIFT = (sizeof(uintptr_t) == 8) ? 3u : 2u;

// Per-size bucket index: the console's `(nSize & ~3) >> 2` is "byte size -> word
// index" for its 4-byte slot; x64 sub_14082D590 spells it `v3 >> 3`. Sizes reaching
// here are already slot-aligned by Allocate/Deallocate, so the mask is redundant on
// both -- kept implicit in the shift.
static inline size_t DogmaSizeToBucket(size_t nSize)
{
    return nSize >> KN_FREE_SLOT_SHIFT;
}

// Bucket count for a given max allocation: (max >> slotShift) + 1 -- x64
// `8 * (a5 >> 3) + 8` bytes.
static inline size_t DogmaBucketCount(size_t nMaxSizeAllocation)
{
    return (nMaxSizeAllocation >> KN_FREE_SLOT_SHIFT) + 1;
}

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
    // optional size word): minimum = max(nOffsetToStoreNext + KN_FREE_SLOT_BYTES,
    // bStoreFreeBlockSize ? nOffsetToStoreSize + KN_FREE_SLOT_BYTES : 0).
    //
    // (x64 widening, Phase-0 regime): the console literal is `+ 4` because a free-item
    // slot is one 32-bit word there. The x64 arbiter widens it -- XB1 sub_140827C60:
    // `v11 = a6 + 8; if ( a7 && a8 + 8 > v11 ) v11 = a8 + 8;`. Keeping the console 4
    // left the minimum at 12, one slot short of the 16 bytes a free item actually needs
    // (next @ +0, size @ +8).
    size_t nMinimumItemSize = (size_t)nOffsetToStoreNextInFreeItem + KN_FREE_SLOT_BYTES;   // v11 = a6 + 8
    if (bStoreFreeBlockSize && (size_t)nOffsetToStoreSizeInFreeItem + KN_FREE_SLOT_BYTES > nMinimumItemSize)
        nMinimumItemSize = (size_t)nOffsetToStoreSizeInFreeItem + KN_FREE_SLOT_BYTES;      // v11 = a8 + 8

    mpaFirstFreeBySize = 0;       // *a1   = 0  (re-assigned below)
    mpFirstPool = 0;              // a1[1] = 0  (re-assigned below)
    mnMinimumAllocationSize = (uint32_t)minSizeAllocation;                     // a1[6] = a4
    mpFirstOutSideAllocation = 0; // a1[9]  = 0
    mnItemsAllocated = 0;         // a1[10] = 0
    mnItemsFreed = 0;             // a1[11] = 0
    mbStoreFreeBlockSize = bStoreFreeBlockSize;                                // a1[7] = a7

    if (minSizeAllocation < nMinimumItemSize)
        mnMinimumAllocationSize = (uint32_t)nMinimumItemSize;                  // a1[6] = v32

    // (x64 widening, Phase-0 regime -- see KN_FREE_SLOT_SHIFT above): a BYTE offset
    // scaled to a free-item SLOT index. Console `>> 2` (4-byte slot); x64
    // sub_140827C60 `*(a1 + 32) = a6 >> 3; *(a1 + 36) = a8 >> 3` (8-byte slot).
    mnOffsetToStoreNext = nOffsetToStoreNextInFreeItem >> KN_FREE_SLOT_SHIFT;   // a1[4] = a6 >> 3
    mnOffsetToStoreSize = nOffsetToStoreSizeInFreeItem >> KN_FREE_SLOT_SHIFT;   // a1[5] = a8 >> 3

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

    // Per-size free-list head array: one head per SLOT-SIZED size bucket up to the
    // max allocation, plus the zero bucket. (x64 widening, Phase-0 regime): both the
    // element width AND the bucket stride widen -- x64 sub_140827C60 allocates
    // `8 * (a5 >> 3) + 8` and memsets `8 * (max >> 3) + 8`, i.e.
    // sizeof(uintptr_t*) * ((maxSize >> 3) + 1); the console is 4 * ((maxSize >> 2) + 1).
    // Only the element width had been widened before: the stride stayed `>> 2`, which
    // merely over-allocated (harmless), but the SAME `>> 2` on the two free-item slot
    // offsets was not harmless -- see KN_FREE_SLOT_SHIFT.
    const size_t nFreeListBytes = sizeof(uintptr_t*) * DogmaBucketCount(maxSizeAllocation);
    mpaFirstFreeBySize = (uintptr_t**)DOGMA_Malloc(nFreeListBytes);

    // First (main) pool.
    DOGMA_MemPool* pFirstPool = (DOGMA_MemPool*)DOGMA_Malloc(mainPoolSizeBytes);
    mpFirstPool = pFirstPool;

    memset(mpaFirstFreeBySize, 0, sizeof(uintptr_t*) * DogmaBucketCount(mnMaxSizeAllocation));

    pFirstPool->SetupPool(0, mainPoolSizeBytes);   // *v36=0; v36[1]=v36[2]=a2-15
}

// ---------------------------------------------------------------------------
// Destructor @ 0x82ADB950
// ---------------------------------------------------------------------------
DOGMA_PoolManager::~DOGMA_PoolManager()
{
    // Free the per-size free-list head array (same byte count it was allocated --
    // sizeof(uintptr_t*) per bucket on x64; see the ctor).
    //
    // The console frees it UNCONDITIONALLY (0x82ADB950: `dword_8324E820(*a1, 4 * ((a1[3] >> 2) + 1))`
    // with no null test) because it never destructs a DEFERRED pool: every X360 DOGMA_PoolManager
    // is heap-constructed at AptInit (AptAllocatorInitialize @0x82ADD118) with real sizes, so
    // mpaFirstFreeBySize is always live. This guard is the MIRROR of the ctor's PC static-init
    // accommodation above -- the (0,0) static AptValueGC_PoolManager takes the ctor's early return
    // with mpaFirstFreeBySize == 0 / mpFirstPool == 0, and its CRT-exit destructor was handing that
    // null straight to DOGMA_FreeSized -> gAptFuncs.pfnMemFreeSize -> AptCallbackMemory::FreeSize/
    // Free -> AptDataHandler::AptFree(null), firing the (faithful) console null-free assert.
    // Deferred construction must have deferred destruction; the pool-chain loop below already
    // mirrors it, this is the same fix for the free-list array.
    if (mpaFirstFreeBySize)
        DOGMA_FreeSized(mpaFirstFreeBySize, sizeof(uintptr_t*) * DogmaBucketCount(mnMaxSizeAllocation));

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
    // ORDER MATTERS and is the x64 arbiter's, not the console's: sub_14082D590 rounds
    // FIRST (`if ( (a2 & 7) != 0 ) v3 = (a2 & ~7) + 8;`) and clamps to the minimum
    // SECOND (`if ( v3 < *(a1 + 40) ) v3 = *(a1 + 40);`). That is only safe because the
    // minimum is itself slot-aligned -- which it is once the ctor's `+ 4` is the
    // arbiter's `+ 8` (see KN_FREE_SLOT_BYTES); with the old 12 the two orders
    // disagreed and the clamped size was not 8-aligned.
    size_t nSize = nAllocatedSize;                          // v3
    if (nSize & (KN_FREE_SLOT_BYTES - 1))
        nSize = (nSize & ~(KN_FREE_SLOT_BYTES - 1)) + KN_FREE_SLOT_BYTES;
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
            uintptr_t* pFree = paBySize[DogmaSizeToBucket(nSize)];     // v7 = (*a1 + 8 * (v3 >> 3))
            if (pFree)
            {
                pResult = pFree;
                // Unlink: head = *(free + 8 * mnOffsetToStoreNext).
                paBySize[DogmaSizeToBucket(nSize)] = (uintptr_t*)pFree[mnOffsetToStoreNext];
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
    // x64 alignment fix (Phase-0 regime -- MUST mirror Allocate, same order, so the
    // per-size free-list buckets match): round to the slot width, then clamp.
    // (x64 sub_140828040 shows only the clamp because its caller's size is already
    // 8-aligned and the compiler folded the round away.)
    size_t nSize = nAllocatedSize;                          // v5
    if (nSize & (KN_FREE_SLOT_BYTES - 1))
        nSize = (nSize & ~(KN_FREE_SLOT_BYTES - 1)) + KN_FREE_SLOT_BYTES;
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
            // *(free + 8 * mnOffsetToStoreNext) = current head.
            pFreed[nNextOffset] = (uintptr_t)paBySize[DogmaSizeToBucket(nSize)];
            // *(free + 8 * mnOffsetToStoreSize) = nSize -- and with mnOffsetToStoreSize
            // now 1 (== gAptValueGCSizeOffset 8 >> 3) this lands at byte +8, exactly where
            // the GC pool walk reads a free item's stride. That is the whole fix.
            if (mbStoreFreeBlockSize)
                pFreed[mnOffsetToStoreSize] = nSize;                   // *(v7 + 8 * *(v9 + 36)) = v10
            paBySize[DogmaSizeToBucket(nSize)] = pFreed;               // head = a2
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
