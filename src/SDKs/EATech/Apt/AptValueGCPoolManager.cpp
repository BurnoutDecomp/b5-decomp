// ===========================================================================
// EATech Apt -- AptValueGC_PoolManager method bodies.
//
// Reconstructed store-for-store from the X360 ARTIST.XEX pseudocode/asm:
//     ctor                       @ 0x82ADBEF8
//     StaticInitialize           @ 0x82ADB7E0
//     DeallocateAptValueGC       @ 0x82AE57D8
//     GetFirstAptValue           @ 0x82AE0DF8
//     GetNextAptValue            @ 0x82AE0BE0
//     `scalar deleting destructor'@ 0x82AE3858  (synthesized by the compiler
//                                                from ~base + operator delete;
//                                                not hand-written here)
// See AptValueGCPoolManager.h for the layout / statics derivation.
// ===========================================================================

#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"

// ---------------------------------------------------------------------------
// Allocator tuning statics (X360 .data). Zero-init; StaticInitialize() sets
// the live values. byte_8324D804 ends up 4 (== sizeof(void*) on X360), which
// selects the AptValueGC_MemItem "Type1" layout (size word at offset +4).
// ---------------------------------------------------------------------------
uint8_t  gAptValueGCSizeOffset    = 0;   // byte_8324D804
uint8_t  gAptValueGCMinItemSize   = 0;   // byte_8324D805
uint8_t  gAptValueGCStoreSizeFlag = 0;   // byte_8324D806
uint32_t gAptValueGCMaxItemSize   = 0;   // dword_8324E2A4

namespace
{
    // The X360 GC walk reads the AptValueGC_MemItem allocated-flag and size
    // word straight off the item, selecting the word by gAptValueGCSizeOffset:
    //   offset 4 -> word = item[1];  offset 0 -> word = item[0];  else 0.
    // The high bit is the "allocated" flag; the low 31 bits are the size.

    inline bool ItemIsAllocated(const uint32_t* pItem)
    {
        uint32_t word;
        if (gAptValueGCSizeOffset == 4)
            word = pItem[1];
        else if (gAptValueGCSizeOffset == 0)
            word = pItem[0];
        else
            return false;                 // LOBYTE(v5) = 0
        return (word >> 31) != 0;
    }

    inline uintptr_t ItemStepSize(const uint32_t* pItem)
    {
        uint32_t word;
        if (gAptValueGCSizeOffset == 4)
            word = pItem[1];
        else if (gAptValueGCSizeOffset == 0)
            word = pItem[0];
        else
            return 0;
        return word & 0x7FFFFFFF;
    }

    // Advance an item cursor by ItemStepSize, which is a BYTE count. The X360 does
    // `add r3, r11, r3` on a raw byte address; do the SAME via byte arithmetic. Plain
    // `pItem += ItemStepSize(pItem)` on a uint32_t* would scale the byte stride by 4.
    inline const uint32_t* AdvanceItem(const uint32_t* pItem)
    {
        return reinterpret_cast<const uint32_t*>(
            reinterpret_cast<const uint8_t*>(pItem) + ItemStepSize(pItem));
    }

    // pool end-of-used == GetFirstItem() + GetBytesUsed()
    //   (asm: pool + pool->mnPoolSize - pool->mnPoolFree + 12)
    inline const uint32_t* PoolItemsBegin(DOGMA_MemPool* pPool)
    {
        return static_cast<const uint32_t*>(pPool->GetFirstItem());
    }

    inline const uint32_t* PoolItemsEnd(DOGMA_MemPool* pPool)
    {
        const uint8_t* p = static_cast<const uint8_t*>(pPool->GetFirstItem());
        return reinterpret_cast<const uint32_t*>(p + pPool->GetBytesUsed());
    }

    inline bool ItemInPool(const uint32_t* pItem, DOGMA_MemPool* pPool)
    {
        return pItem >= PoolItemsBegin(pPool) && pItem < PoolItemsEnd(pPool);
    }
}

// ---------------------------------------------------------------------------
// ctor @ 0x82ADBEF8
//
// Forwards to DOGMA_PoolManager(mainPool, overflowPool, minSize=D805,
// maxSize=E2A4, nOffNext=D806, bStoreFreeBlockSize=1, nOffSize=D804,
// bTrackOutside=1). The literal `1`s are the asm's li r3,1 (var_19 store) and
// li r9,1.
// ---------------------------------------------------------------------------
AptValueGC_PoolManager::AptValueGC_PoolManager(size_t mainPoolSizeBytes,
                                               size_t overflowPoolSizeBytes)
    : DOGMA_PoolManager(mainPoolSizeBytes,
                        overflowPoolSizeBytes,
                        gAptValueGCMinItemSize,    // byte_8324D805
                        gAptValueGCMaxItemSize,    // dword_8324E2A4
                        gAptValueGCStoreSizeFlag,  // byte_8324D806 (nOffNext)
                        true,                      // bStoreFreeBlockSize = 1
                        gAptValueGCSizeOffset,     // byte_8324D804 (nOffSize)
                        true)                      // bTrackOutside = 1
{
}

// ---------------------------------------------------------------------------
// StaticInitialize @ 0x82ADB7E0
//
// byte_8324D806 = 0; byte_8324D804 = 4; then scan byte_82144A18[1..37] for the
// min/max object size -> dword_8324E2A4 = max; byte_8324D805 = min.
// (min seed = 1000000 == 0xF4240; loop index < 38.)
//
// x64 (Burnout_External_Xbox_One sub_14082D9F0, the arbiter): byte_141479FB3 = 0
// (the store-next offset stays 0) but byte_141479FB6 = **8** -- the size/alloc-flag
// offset widens with the pointer so the AptValueGC_MemItem alloc bit + freed-size
// slot live in the qword AFTER the vptr (bytes 8..15), exactly matching the
// DOGMA free-list layout (next @ word 0 over the vptr, size @ +8). Keeping the
// console's 4 here made SetIsAllocated/GetSize match NEITHER union arm (silent
// no-ops) -- fixed to sizeof(void*) == 8, 2026-07-02.
// ---------------------------------------------------------------------------
void AptValueGC_PoolManager::StaticInitialize()
{
    gAptValueGCStoreSizeFlag = 0;
    gAptValueGCSizeOffset = 8;   // x64: XB1 byte_141479FB6 = 8 (was 4 on the consoles)

    uint32_t nMax = 0;
    uint32_t nMin = 1000000;
    for (int i = 1; i < AptVFT_NumVFTs; ++i)
    {
        uint32_t nSize = byte_82144A18[i];
        if (nSize > nMax)
            nMax = nSize;
        if (nSize < nMin)
            nMin = nSize;
    }

    gAptValueGCMaxItemSize = nMax;
    gAptValueGCMinItemSize = (uint8_t)nMin;
}

// ---------------------------------------------------------------------------
// AllocateAptValueGC
//
// DOGMA Allocate, then set the MemItem allocated flag. The alloc-side mirror of
// DeallocateAptValueGC; the X360 inlines this pair into every GC value type's
// `operator new` (e.g. AptArray @0x82AE6088 / AptGlobalExtensionObject
// @0x82AE6588: Allocate(off_8324D834, size) ; SetIsAllocated(p, byte_8324D804,
// 1)). The flag is set unconditionally (unlike the free, which gates the clear
// on a successful Deallocate).
// ---------------------------------------------------------------------------
void* AptValueGC_PoolManager::AllocateAptValueGC(size_t nAllocatedSize)
{
    void* pItem = Allocate(nAllocatedSize);
    reinterpret_cast<AptValueGC_MemItem*>(pItem)
        ->SetIsAllocated(gAptValueGCSizeOffset, true);
    return pItem;
}

// ---------------------------------------------------------------------------
// DeallocateAptValueGC @ 0x82AE57D8
//
// r31 = pItem. DOGMA Deallocate; on success clear the MemItem allocated flag.
// ---------------------------------------------------------------------------
bool AptValueGC_PoolManager::DeallocateAptValueGC(void* pItem, size_t nAllocatedSize)
{
    bool bFreed = Deallocate(pItem, nAllocatedSize);
    if (bFreed)
    {
        reinterpret_cast<AptValueGC_MemItem*>(pItem)
            ->SetIsAllocated(gAptValueGCSizeOffset, false);
    }
    return bFreed;
}

// ---------------------------------------------------------------------------
// GetFirstAptValue @ 0x82AE0DF8
//
// Walk pools (mpFirstPool -> next ...). Within a pool, step item-by-item from
// GetFirstItem(): the first item whose allocated flag is set is returned (as
// an AptValue*). The step distance is the current item's size; an unallocated
// item with size 0 is impossible here because the loop only advances inside a
// pool's used range. Returns null when no live item exists.
// ---------------------------------------------------------------------------
AptValue* AptValueGC_PoolManager::GetFirstAptValue()
{
    DOGMA_MemPool* pPool = GetFirstPool();
    do
    {
        for (const uint32_t* pItem = PoolItemsBegin(pPool);
             ItemInPool(pItem, pPool);
             pItem = AdvanceItem(pItem))
        {
            if (ItemIsAllocated(pItem))
                return reinterpret_cast<AptValue*>(const_cast<uint32_t*>(pItem));
        }
        pPool = pPool ? pPool->GetNextPool() : nullptr;
    }
    while (pPool);

    return nullptr;
}

// ---------------------------------------------------------------------------
// GetNextAptValue @ 0x82AE0BE0
//
// Find the pool containing pCurrent, step forward to the next allocated item
// (crossing into following pools as needed). When the pool walk is exhausted,
// fall back to the first tracked outside-allocation (if outside tracking is on
// and one exists), returning its user pointer (+8 past the two list links).
// Returns null when nothing follows.
// ---------------------------------------------------------------------------
AptValue* AptValueGC_PoolManager::GetNextAptValue(AptValue* pCurrent)
{
    const uint32_t* pCur = reinterpret_cast<const uint32_t*>(pCurrent);

    // Locate the pool that holds pCurrent.
    DOGMA_MemPool* pPool = GetFirstPool();
    while (pPool && !ItemInPool(pCur, pPool))
        pPool = pPool->GetNextPool();

    if (!pPool)
    {
        // pCurrent is an outside allocation: the list link sits 8 bytes before
        // the returned pointer; its prev/next chain head is *(pCur - 2).
        const uint32_t* pNode = *reinterpret_cast<const uint32_t* const*>(pCur - 2);
        if (pNode)
            return reinterpret_cast<AptValue*>(
                const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(pNode) + 8));
        return nullptr;
    }

    // Step to the slot following pCurrent within its pool, then scan forward
    // (across pools) for the next allocated item.
    const uint32_t* pItem = AdvanceItem(pCur);

    while (pPool)
    {
        while (ItemInPool(pItem, pPool))
        {
            if (ItemIsAllocated(pItem))
                return reinterpret_cast<AptValue*>(const_cast<uint32_t*>(pItem));
            pItem = AdvanceItem(pItem);
        }
        pPool = pPool->GetNextPool();
        if (pPool)
            pItem = PoolItemsBegin(pPool);
    }

    // Pools exhausted -> first outside allocation, if tracked.
    if (!GetTracksOutsideAllocations())
        return nullptr;

    const void* pFirstOutside = GetFirstOutsideAllocationRaw();
    if (pFirstOutside)
        return reinterpret_cast<AptValue*>(
            const_cast<uint8_t*>(static_cast<const uint8_t*>(pFirstOutside) + 8));
    return nullptr;
}

// ===========================================================================
// The two GC-pool introspection accessors (HOMED 2026-07-02, retiring the
// AptRenderLinkStubs nulls -- the plan's Phase-1 GC-substrate list items).
//   * GetAllocatedCount: the live-allocation counter the DOGMA base maintains
//     (mnItemsAllocated -- the X360 callers read *(pool + 0x28)).
//   * GetAllAllocatedAptValues: the caller pairs the SAME pool walk CleanAll
//     uses (GetFirstAptValue/GetNextAptValue) into a flat snapshot; the
//     console returns its internal live-table pointer, which the DOGMA port
//     does not keep as a flat array -- snapshot into a static scratch sized
//     by the live count (single-threaded bring-up; the sole caller,
//     AptReplaceReferences' zombie-survivor fixup, consumes it immediately).
// ===========================================================================
int AptValueGCPool_GetAllocatedCount(void* pPool)
{
    return static_cast<int>(
        static_cast<AptValueGC_PoolManager*>(pPool)->mnItemsAllocated);
}

void** AptValueGC_PoolManager_GetAllAllocatedAptValues(void* pPool)
{
    AptValueGC_PoolManager* const pMgr =
        static_cast<AptValueGC_PoolManager*>(pPool);
    static void** spSnapshot = nullptr;
    static size_t snCapacity = 0;
    const size_t nCount = pMgr->mnItemsAllocated;
    if (nCount > snCapacity)
    {
        delete[] spSnapshot;
        spSnapshot = new void*[nCount ? nCount : 1];
        snCapacity = nCount ? nCount : 1;
    }
    size_t i = 0;
    for (AptValue* p = pMgr->GetFirstAptValue(); p != nullptr && i < snCapacity;
         p = pMgr->GetNextAptValue(p))
        spSnapshot[i++] = p;
    return spSnapshot;
}
