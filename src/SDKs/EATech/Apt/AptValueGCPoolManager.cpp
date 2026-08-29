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
        if (gAptValueGCSizeOffset == sizeof(void*))     // x64: 8 (byte_141479FB6)
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
        // x64 sub_140838950: the allocated flag is BIT 0 of the size word
        // (== AptValue::mbIsAllocated when the item is a live value).
        return (ItemSizeWord(pItem) & 1u) != 0;
    }

    inline uintptr_t ItemStepSize(const uint32_t* pItem)
    {
        // x64: size = word & ~1 (GetSize @0x140839B80 `and rax,...FFFEh`).
        return ItemSizeWord(pItem) & ~static_cast<uintptr_t>(1);
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
// The per-VFT object-size table CONTENTS (byte_82144A18; storage in
// AptGlobals.cpp). The X360 .rdata bytes are RECOVERED (0x82144A18 dump):
//   00 04 00 08 0c 0c 0c 0c 0c 24 20 08 28 20 2c 20 20 20 24 20
//   20 20 20 20 20 20 20 20 20 40 10 20 20 28 24 34 34 44
// -- the CONSOLE 32-bit per-type allocation sizes, cross-checked in-tree:
// [12] CharacterInstHandle 0x28 == AptCIH::operator new(40), [14] Array 0x2C
// == AptArray::operator new(44). The console links these bytes at build time
// (class-set-generated); the x64 twin (XB1 sub_14082D9F0) scans its OWN
// x64-size table (byte_140C13B48 -- x64 sizes, contents not in the export
// set), so the host regenerates the contents by the vendor's rule -- sizeof()
// over the reconstructed class set -- filled by StaticInitialize before its
// scan (live C++ objects, not a serialized image; the console byte is cited
// per entry below).
//
// Entries with no reconstructed class stay 0: a zero entry is inside the
// shipped scan's behaviour (console [2] Property == 0 IS scanned, so the
// shipped min is 0x00; the DOGMA ctor clamps a below-bookkeeping minimum).
// [29] Extension (console 0x40) is the dynamic-size type -- GetNextAptValue
// special-cases it to read the per-item size, so its entry is scan-input only.
// ---------------------------------------------------------------------------
namespace
{
    template <typename T>
    inline uint8_t VFTObjectSize()
    {
        static_assert(sizeof(T) <= 0xFF, "per-VFT size-table entry must fit its byte slot");
        return static_cast<uint8_t>(sizeof(T));
    }

    void PopulateVFTObjectSizeTable()
    {
        // [0] AptVFT_xxx: console 0x00 (never allocated; outside the scan).
        byte_82144A18[AptVFT_StringValue]         = VFTObjectSize<AptString>();          // console 0x04 (non-GC pooled; Create() sizes the real alloc)
        // [2] AptVFT_Property: console 0x00 (the shipped zero the min scan lands on).
        byte_82144A18[AptVFT_None]                = VFTObjectSize<AptNone>();            // console 0x08
        byte_82144A18[AptVFT_Register]            = VFTObjectSize<AptRegister>();        // console 0x0C
        byte_82144A18[AptVFT_Boolean]             = VFTObjectSize<AptBoolean>();         // console 0x0C
        byte_82144A18[AptVFT_Float]               = VFTObjectSize<AptFloat>();           // console 0x0C
        byte_82144A18[AptVFT_Integer]             = VFTObjectSize<AptInteger>();         // console 0x0C
        byte_82144A18[AptVFT_Lookup]              = VFTObjectSize<AptLookup>();          // console 0x0C
        byte_82144A18[AptVFT_NativeFunction]      = VFTObjectSize<AptNativeFunction>();  // console 0x24
        byte_82144A18[AptVFT_FrameStack]          = VFTObjectSize<AptFrameStack>();      // console 0x20
        byte_82144A18[AptVFT_Extern]              = VFTObjectSize<AptExtern>();          // console 0x08
        byte_82144A18[AptVFT_CharacterInstHandle] = VFTObjectSize<AptCIH>();             // console 0x28 (== AptCIH::operator new(40))
        byte_82144A18[AptVFT_Sound]               = VFTObjectSize<AptSound>();           // console 0x20
        byte_82144A18[AptVFT_Array]               = VFTObjectSize<AptArray>();           // console 0x2C (== AptArray::operator new(44))
        byte_82144A18[AptVFT_Math]                = VFTObjectSize<AptMathObj>();         // console 0x20
        byte_82144A18[AptVFT_Key]                 = VFTObjectSize<AptKey>();             // console 0x20
        byte_82144A18[AptVFT_Global]              = VFTObjectSize<AptGlobal>();          // console 0x20
        byte_82144A18[AptVFT_ScriptColour]        = VFTObjectSize<AptScriptColour>();    // console 0x24
        byte_82144A18[AptVFT_Object]              = VFTObjectSize<AptObject>();          // console 0x20
        byte_82144A18[AptVFT_Prototype]           = VFTObjectSize<AptPrototype>();       // console 0x20
        byte_82144A18[AptVFT_Date]                = VFTObjectSize<AptDate>();            // console 0x20
        byte_82144A18[AptVFT_MovieClip]           = VFTObjectSize<AptMovieClip>();       // console 0x20
        // [23] AptVFT_Mouse: console 0x20 -- no reconstructed class; never allocated on the host.
        byte_82144A18[AptVFT_XmlNode]             = VFTObjectSize<AptXmlNode>();         // console 0x20
        byte_82144A18[AptVFT_Xml]                 = VFTObjectSize<AptXml>();             // console 0x20
        // [26] AptVFT_XmlAttributes: console 0x20 -- no reconstructed class.
        // [27] AptVFT_LoadVars: console 0x20 -- no reconstructed class.
        byte_82144A18[AptVFT_TextFormat]          = VFTObjectSize<AptTextFormat>();      // console 0x20
        // [29] AptVFT_Extension: console 0x40 -- dynamic-size (the walk reads the per-item size).
        byte_82144A18[AptVFT_GlobalExtension]     = VFTObjectSize<AptGlobalExtensionObject>(); // console 0x10
        byte_82144A18[AptVFT_Stage]               = VFTObjectSize<AptStage>();           // console 0x20
        byte_82144A18[AptVFT_Error]               = VFTObjectSize<AptError>();           // console 0x20
        byte_82144A18[AptVFT_StringObject]        = VFTObjectSize<AptStringObject>();    // console 0x28
        byte_82144A18[AptVFT_ScriptFunction1]     = VFTObjectSize<AptScriptFunction1>(); // console 0x24
        byte_82144A18[AptVFT_ScriptFunction2]     = VFTObjectSize<AptScriptFunction2>(); // console 0x34
        byte_82144A18[AptVFT_ScriptFunctionByteCodeBlock] = VFTObjectSize<AptScriptFunctionByteCodeBlock>(); // console 0x34
        byte_82144A18[AptVFT_CIHNone]             = VFTObjectSize<AptCIHNone>();         // console 0x44
    }
}

// ---------------------------------------------------------------------------
// StaticInitialize @ 0x82ADB7E0
//
// byte_8324D806 = 0; byte_8324D804 = 4; then scan byte_82144A18[1..37] for the
// min/max object size -> dword_8324E2A4 = max; byte_8324D805 = min.
// (min seed = 1000000 == 0xF4240; loop index < 38.) The console scan result is
// ground truth (0x82144A18 dump): the shipped statics are min 0x00 (entry [2]
// Property is 0 and IS scanned -- dossier 0x82ADB7E0 has no zero-skip) / max
// 0x44 ([37] CIHNone); the same faithful scan over the regenerated x64 table
// yields the x64-widened pair (retiring AptInit's old 4/256 override FLAG).
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
    // Fill byte_82144A18 (the console's .rdata image is class-set-generated at
    // build time; the x64 generation runs here, before the scan reads it).
    PopulateVFTObjectSizeTable();

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
        // pCurrent is an outside allocation: the TWO-POINTER list node sits
        // 2*sizeof(void*) before the user pointer, and the user pointer is
        // node + 2*sizeof(void*) (x64 sub_1408394C0: `v4 = *(a2 - 16);
        // return v4 + 16;`; console: -8/+8).
        const uint8_t* pNode = *reinterpret_cast<const uint8_t* const*>(
            reinterpret_cast<const uint8_t*>(pCur) - 2 * sizeof(void*));
        if (pNode)
            return reinterpret_cast<AptValue*>(
                const_cast<uint8_t*>(pNode + 2 * sizeof(void*)));
        return nullptr;
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
        return nullptr;

    const void* pFirstOutside = GetFirstOutsideAllocationRaw();
    if (pFirstOutside)
        return reinterpret_cast<AptValue*>(const_cast<uint8_t*>(
            static_cast<const uint8_t*>(pFirstOutside) + 2 * sizeof(void*)));   // x64: node + 16
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
// The console reads *(pool+0x28) (mnItemsAllocated) INLINE; this type-erased wrapper keeps
// the DOGMA pool layout opaque to its caller (AptAnimationTarget.cpp).
// FLAG PC-platform leaf: header-decoupling wrapper, no console counterpart.
int AptValueGCPool_GetAllocatedCount(void* pPool)
{
    return static_cast<int>(
        static_cast<AptValueGC_PoolManager*>(pPool)->mnItemsAllocated);
}

// ⛔ CORRECTED 2026-08-28: this was FLAGged "PC-platform leaf: port-only reconstruction, no console
// counterpart". That claim is FALSE. The console counterpart is the x64 `sub_140838090`, which
// allocates `8 * mnItemsAllocated` and runs exactly this walk (pool walk first, then the outside
// allocations appended). The old note also asserted "the DOGMA pool keeps no flat live-array" as a
// reason none could exist -- but the console does not keep one either; it builds the same snapshot.
// Snapshotting into a static scratch buffer is therefore the console's own shape, not a port-only
// invention.
// ⚠️ progress/faithfulness_baseline.json still carries an `apt_shim` suppression for this symbol.
// That entry is now questionable and should be re-adjudicated by whoever next reconciles the
// baseline -- it is NOT corrected here, because the ledger is CI-reconciled and blind-regenerating
// it is its own hazard.
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
    if (pnOutCount != nullptr)
        *pnOutCount = static_cast<int>(i);
    return spSnapshot;
}
