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

// ---- the reconstructed AptValue class set (per-VFT size-table generation:
//      StaticInitialize fills byte_82144A18 from sizeof() -- see the note there) ----
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"
#include "SDKs/EATech/include/Apt/AptValue/AptNone.h"
#include "SDKs/EATech/include/Apt/AptValue/AptRegister.h"
#include "SDKs/EATech/include/Apt/AptValue/AptBoolean.h"
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"
#include "SDKs/EATech/include/Apt/AptValue/AptLookup.h"
#include "SDKs/EATech/include/Apt/AptValue/AptExtern.h"
#include "SDKs/EATech/include/Apt/AptValue/AptStringObject.h"
#include "SDKs/EATech/include/Apt/AptNativeFunction.h"
#include "SDKs/EATech/include/Apt/AptFrameStack.h"
#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCIHNone.h"
#include "SDKs/EATech/include/Apt/AptSound.h"
#include "SDKs/EATech/include/Apt/AptArray.h"
#include "SDKs/EATech/include/Apt/AptMathObj.h"
#include "SDKs/EATech/include/Apt/AptKey.h"
#include "SDKs/EATech/include/Apt/AptGlobal.h"
#include "SDKs/EATech/include/Apt/AptScriptColour.h"
#include "SDKs/EATech/include/Apt/AptObject.h"
#include "SDKs/EATech/include/Apt/AptPrototype.h"
#include "SDKs/EATech/include/Apt/AptDate.h"
#include "SDKs/EATech/include/Apt/AptMovieClip.h"
#include "SDKs/EATech/include/Apt/AptXmlNode.h"
#include "SDKs/EATech/include/Apt/AptXml.h"
#include "SDKs/EATech/include/Apt/AptTextFormat.h"
#include "SDKs/EATech/include/Apt/AptGlobalExtensionObject.h"
#include "SDKs/EATech/include/Apt/AptStage.h"
#include "SDKs/EATech/include/Apt/AptError.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction1.h"
#include "SDKs/EATech/include/Apt/AptScriptFunction2.h"
#include "SDKs/EATech/include/Apt/AptScriptFunctionByteCodeBlock.h"

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

    inline uintptr_t ItemSizeWord(const uint32_t* pItem)
    {
        if (gAptValueGCSizeOffset == sizeof(void*))
            return *reinterpret_cast<const uintptr_t*>(
                reinterpret_cast<const uint8_t*>(pItem) + sizeof(void*));
        if (gAptValueGCSizeOffset == 0)
            return *reinterpret_cast<const uintptr_t*>(pItem);
        return 0;
    }

    inline bool ItemIsAllocated(const uint32_t* pItem)
    {
        if (gAptValueGCSizeOffset != sizeof(void*) && gAptValueGCSizeOffset != 0)
            return false;                 // LOBYTE(v5) = 0
        // ARTIST 82AE0DF8 tests the high bit.
        return (ItemSizeWord(pItem) & 0x80000000u) != 0;
    }

    inline uintptr_t ItemStepSize(const uint32_t* pItem)
    {
        // ARTIST masks off the high allocation bit.
        return ItemSizeWord(pItem) & 0x7fffffffu;
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
// ARTIST's per-value-type allocation strides at 0x82144A18. Type 29
// uses its own dynamic size during traversal; the table still participates in
// the minimum and maximum scan.
// ---------------------------------------------------------------------------
namespace
{
    void PopulateVFTObjectSizeTable()
    {
        // ARTIST 0x82144A18: byte strides indexed by Apt value type.
        static const uint8_t kNativeSizes[] = {
            0x00, 0x04, 0x00, 0x08, 0x0c, 0x0c, 0x0c, 0x0c,
            0x0c, 0x24, 0x20, 0x08, 0x28, 0x20, 0x2c, 0x20,
            0x20, 0x20, 0x24, 0x20, 0x20, 0x20, 0x20, 0x20,
            0x20, 0x20, 0x20, 0x20, 0x40, 0x10, 0x20, 0x20,
            0x28, 0x24, 0x34, 0x34, 0x44, 0x28
        };
        CGS_STATIC_ASSERT(sizeof(kNativeSizes) == AptVFT_NumVFTs, "X360 Apt value size table");
        for (int i = 0; i < AptVFT_NumVFTs; ++i)
            byte_82144A18[i] = kNativeSizes[i];
    }

}

// ---------------------------------------------------------------------------
// StaticInitialize @ 0x82ADB7E0: configure the native size-word offset and
// scan the original 38-entry value-size table for pool limits.
// ---------------------------------------------------------------------------
void AptValueGC_PoolManager::StaticInitialize()
{
    // Materialize the original table before the scan.
    PopulateVFTObjectSizeTable();

    gAptValueGCStoreSizeFlag = 0;
    gAptValueGCSizeOffset = 4;   // ARTIST byte_8324D804

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
        pPool = pPool ? pPool->GetNextPool() : 0;
    }
    while (pPool);

    return 0;
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
        // pCurrent is an outside allocation: the TWO-POINTER list node sits
        // 2*sizeof(void*) before the user pointer, and the user pointer is
        // node + 2*sizeof(void*) (x64 sub_1408394C0: `v4 = *(a2 - 16);
        // return v4 + 16;`; console: -8/+8).
        const uint8_t* pNode = *reinterpret_cast<const uint8_t* const*>(
            reinterpret_cast<const uint8_t*>(pCur) - 2 * sizeof(void*));
        if (pNode)
            return reinterpret_cast<AptValue*>(
                const_cast<uint8_t*>(pNode + 2 * sizeof(void*)));
        return 0;
    }

    // Step past pCurrent: an ALLOCATED item's word at the size offset is the live
    // AptValue bitfield (not a size) -- both binaries step it by the per-type size
    // table (X360 0x82AE0BE0 byte_82144A18[type]; x64 sub_1408394C0
    // byte_140C13B48[type]), with type 29 == AptVFT_Extension reading its dynamic
    // size at item qword +3 (x64 *(a2+24)). Only a FREE item steps by its size word.
    const uint32_t* pItem;
    if (ItemIsAllocated(pCur))
    {
        const int nType = static_cast<int>(
            reinterpret_cast<const AptValue*>(pCur)->getVtblIndex());
        const uintptr_t nStep = (nType == AptVFT_Extension)
            ? *reinterpret_cast<const uintptr_t*>(
                  reinterpret_cast<const uint8_t*>(pCur) + 3 * sizeof(void*))
            : byte_82144A18[nType];
        pItem = reinterpret_cast<const uint32_t*>(
            reinterpret_cast<const uint8_t*>(pCur) + nStep);
    }
    else
    {
        pItem = AdvanceItem(pCur);
    }

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
        return 0;

    const void* pFirstOutside = GetFirstOutsideAllocationRaw();
    if (pFirstOutside)
        return reinterpret_cast<AptValue*>(const_cast<uint8_t*>(
            static_cast<const uint8_t*>(pFirstOutside) + 2 * sizeof(void*)));   // x64: node + 16
    return 0;
}

//
// CORRECTED AGAIN 2026-08-29 (independent fix, merged at rebase): the caller used to pair this
// snapshot with mnItemsAllocated as the element count, but the two need not agree --
// mnItemsAllocated counts every DOGMA pool item while the GetFirstAptValue/GetNextAptValue walk
// visits only live AptValues, and the walk can also stop early. ReplaceReferences then read the
// uninitialised tail of the scratch buffer as AptValue* and dereferenced garbage. The snapshot now
// reports the number of entries it actually wrote through *pnOutCount, which is the only count the
// caller may use. (Latent since this leaf was homed: CleanRemList's survivor pass was unreachable
// until the AptAnimationTarget refcount bit-position fix of the same date made it live. The x64
// twin sub_140838090 RETURNS its own count the same way -- the out-param is the calling-convention
// port of that return pair.)
void** AptValueGC_PoolManager_GetAllAllocatedAptValues(void* pPool, int* pnOutCount)
{
    AptValueGC_PoolManager* const pMgr =
        static_cast<AptValueGC_PoolManager*>(pPool);
    static void** spSnapshot = 0;
    static size_t snCapacity = 0;
    const size_t nCount = pMgr->mnItemsAllocated;
    if (nCount > snCapacity)
    {
        delete[] spSnapshot;
        spSnapshot = new void*[nCount ? nCount : 1];
        snCapacity = nCount ? nCount : 1;
    }
    size_t i = 0;
    for (AptValue* p = pMgr->GetFirstAptValue(); p != 0 && i < snCapacity;
         p = pMgr->GetNextAptValue(p))
        spSnapshot[i++] = p;
    if (pnOutCount != 0)
        *pnOutCount = static_cast<int>(i);
    return spSnapshot;
}
