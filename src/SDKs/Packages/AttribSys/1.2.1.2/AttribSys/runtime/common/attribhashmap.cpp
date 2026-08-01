// attribhashmap.cpp -- Attrib::HashMap open-addressing attribute hash map bodies.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (AttribSys 1.2.1.2). The
// container/Node layout is pinned in attribhashmap.h.
//
// ⛔⛔ KNOWN DEFECT, NOT FIXED HERE (2026-08-01): Remove / Transfer / UpdateSearchLength
// still address this object and its buckets through raw CONSOLE byte offsets --
// `*(u16*)(this+4)` for muCapacity, `+6` muCount, `+0xA` mu8MaxSearchLength, `+0xB`
// mu8KeyShift, and a `<< 4` / `>> 4` bucket stride. Every one of those is WRONG on x64:
// mpBuckets is a pointer, so the header fields sit 4 bytes later, and sizeof(Node) is 24,
// not 16. Release @0x82802718 had the identical bug (`*(u16*)(this+8)` == muCapacity on the
// host, not muRefCount) and it cost this project a whole boot -- "Too many releases of
// object!" the moment DirectorResourceManager::Prepare gave the SDK its first real
// collection to reference-count. Release is fixed by name below; the removal trio is NOT,
// because nothing on the committed boot path removes an attribute and rewriting it deserves
// its own verified pass. If you land ANY attribute removal / table repair, fix these first.
// The already-correct half (HashMap / Add / RebuildTable / Find / FindIndex / GetKeyAtIndex /
// PreFlightAdd / CountSearchCacheLines) reads every member BY NAME -- copy that.
//
//   HashMap::HashMap            @ 0x828094E8
//   HashMap::CountSearchCacheLines @ 0x828048C8
//   HashMap::Find               @ 0x82804838
//   HashMap::FindIndex          @ 0x82804700
//   HashMap::GetKeyAtIndex      @ 0x82802660
//   HashMap::PreFlightAdd       @ 0x828027A8
//   HashMap::Release            @ 0x82802718
//   HashMap::Remove             @ 0x82807A78
//   HashMap::Transfer           @ 0x828049D8
//   HashMap::UpdateSearchLength @ 0x82804B50

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribhashmap.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // Free (teardown bucket release)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"            // Attrib::Database (Node type-index resolve)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"   // Attrib::TypeDesc
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"  // Attrib::Alloc (RebuildTable carve)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"           // Attrib::TableFreeFunc (old-array census release)
#include <new>   // placement new (in-place Node construction in Add)

// ~ClassPrivate's bucket release (@0x8280F4D8 tail): free the bucket array by
// its capacity (X360 capacity << 4 == capacity * node stride; sizeof-based for
// the widened x64 node).
void Attrib::HashMap::ReleaseBucketsForTeardown()
{
    if (mpBuckets != NULL)
        Attrib::HashMapTablePolicy::Free(mpBuckets, muCapacity * sizeof(Node));
}

// ============================================================================
// Attrib::HashMap::HashMap @ 0x828094E8
// ============================================================================
// Seed the counters, stash the rotate shift, then -- unless the requested count is 0 --
// size and build the initial bucket array. A non-dynamic (fixed) map first rounds the
// caller's count UP through the TablePolicy grow curve: ((20*(n-1))/16 + 3) & ~3
// (with n==1 mapping to 1). The X360 uses signed /16 (srawi + addze).
Attrib::HashMap::HashMap(unsigned int luCount, u8 lu8KeyShift, u8 lu8Dynamic)
{
    mu8KeyShift        = lu8KeyShift;
    mpBuckets          = NULL;
    muCapacity         = 0;
    muCount            = 0;
    muRefCount         = 0;
    mu8MaxSearchLength = 0;

    if (luCount != 0)
    {
        if (lu8Dynamic == 0)
        {
            if (luCount == 1)
            {
                luCount = 1;
            }
            else
            {
                const s32 liGrown = ((20 * static_cast<s32>(luCount - 1)) >> 4) + 3;
                luCount = static_cast<u32>(liGrown) & 0xFFFFFFFCu;
            }
        }
        RebuildTable(luCount);
    }
}

// ============================================================================
// Attrib::HashMap::Add @ 0x82809580
// ============================================================================
// Insert one (key -> payload) node. If the table is exactly full (live count ==
// capacity) grow-and-rehash it first through the TablePolicy curve ((20*cap)/16 + 3)
// & ~3, or 1 when the table is still empty. Home the key by rotate-hash, then
// PreFlightAdd to find the first free slot along its probe run (0xFFFFFFFF when the key
// is already present, or >= capacity when no free slot -> return false). Placement-
// construct the node there, then fold the probe cost back into the home bucket's cached
// run length (mu8SearchLen) and the table-wide worst-collision high-water mark
// (mu8MaxSearchLength). Bump the live count, and -- unless the caller passed lbNoGrow --
// grow-and-rehash once more if the worst run now exceeds 16.
bool Attrib::HashMap::Add(u64 luKey, u64 luTypeKey, void* lpValue, bool lbLaidOut,
                          u8 lu8Flags, bool lbNoGrow, void* lpBase)
{
    // Grow-and-rehash when the table is exactly full. The X360 uses signed /16
    // (srawi + addze); for the always-positive capacity it is a plain truncating divide.
    if (muCount == muCapacity)
    {
        unsigned int luGrow;
        if (muCapacity != 0)
        {
            const s32 liGrown = ((20 * static_cast<s32>(muCapacity)) >> 4) + 3;
            luGrow = static_cast<u32>(liGrown) & 0xFFFFFFFCu;
        }
        else
        {
            luGrow = 1;
        }
        RebuildTable(luGrow);
    }

    const u8  lu8KeyShift = mu8KeyShift;
    const u32 luCapacity  = muCapacity;

    // Home index = rotl32(key, keyShift) % capacity (the X360 twllei is the divide guard).
    const u32 luHome = static_cast<u32>((luKey >> (64 - lu8KeyShift)) |
                                        (luKey << lu8KeyShift)) % luCapacity;

    int liSteps = 0;
    const unsigned int luDest = PreFlightAdd(luKey, luHome, &liSteps);
    if (luDest >= luCapacity)
        return false;

    // Placement-construct the node in the selected free slot.
    Node* lpDest = &mpBuckets[luDest];
    if (lpDest)
        new (lpDest) Node(luKey, luTypeKey, lpValue, lbLaidOut, lu8Flags, lpBase);

    // Fold the probe cost into the home bucket's cached run length (max of the two).
    const u8 lu8Steps = static_cast<u8>(liSteps);
    Node* lpHome = &mpBuckets[luHome];
    if (lpHome->mu8SearchLen <= lu8Steps)
        lpHome->mu8SearchLen = lu8Steps;

    // Raise the table-wide worst-collision mark if this run beat it.
    if (static_cast<unsigned int>(liSteps) > mu8MaxSearchLength)
        mu8MaxSearchLength = lu8Steps;

    const unsigned int luWorst = mu8MaxSearchLength;
    ++muCount;

    // Grow-and-rehash again if the worst run now exceeds 16, unless the caller vetoed it.
    if (luWorst > 0x10 && !lbNoGrow)
    {
        if (muCapacity != 0)
        {
            const s32 liGrown = ((20 * static_cast<s32>(muCapacity)) >> 4) + 3;
            RebuildTable(static_cast<u32>(liGrown) & 0xFFFFFFFCu);
        }
        else
        {
            RebuildTable(1);
        }
    }
    return true;
}

// ============================================================================
// Attrib::HashMap::RebuildTable @ 0x82807C18 (attrib-sdk wave 2026-07-27;
// recovered via headless IDA -- absent from .ida-exports)
// ============================================================================
// Grow (or first-build) the bucket array: stash the old {table, capacity},
// install the new capacity with zeroed count + worst-collision mark, carve the
// new array from the AttribSys allocator ("Attrib::HashMapTable" tag), clear
// every new bucket to the free state (key 0, value self-homed, flags 0),
// Transfer every live old entry across, then census-release the old buffer
// (the X360 frees capacity*16 through the package allocator's heap and bumps
// the table-policy freed-byte counter; sizeof-based for the widened node).
void Attrib::HashMap::RebuildTable(unsigned int luCount)
{
    if (luCount == 0)
        return;

    Node* lpOldTable = mpBuckets;
    const u16 lu16OldCapacity = muCapacity;

    muCapacity = static_cast<u16>(luCount);
    muCount = 0;
    mu8MaxSearchLength = 0;

    // X360 Alloc((16 * count) & 0xFFFF0, "Attrib::HashMapTable") -- the mask is
    // the u16-capacity clamp of the byte size; sizeof-based on x64.
    mpBuckets = static_cast<Node*>(
        Attrib::Alloc(sizeof(Node) * muCapacity, "Attrib::HashMapTable"));
    for (u16 lu16Slot = 0; lu16Slot < muCapacity; ++lu16Slot)
    {
        Node* lpNode = &mpBuckets[lu16Slot];
        lpNode->mKey = 0;
        lpNode->mpValue = lpNode;   // free bucket self-homes its value slot
        lpNode->mTypeIndex = 0;
        lpNode->mu8SearchLen = 0;
        lpNode->mFlags = 0;
    }

    if (lpOldTable != NULL)
    {
        for (u16 lu16Slot = 0; lu16Slot < lu16OldCapacity; ++lu16Slot)
        {
            Node& lrOld = lpOldTable[lu16Slot];
            if (lrOld.IsOccupied())
            {
                lrOld.mu8SearchLen = 0;
                Transfer(lrOld);
            }
        }

        // Census-release the old array (the X360 decrements the live-byte
        // census, refreshes the peak, then frees through the package
        // allocator's heap and bumps the table-policy freed-byte counter --
        // the committed TableFreeFunc is exactly that sequence).
        Attrib::TableFreeFunc(lpOldTable, sizeof(Node) * lu16OldCapacity);
    }
}

// ============================================================================
// Attrib::Node::Node @ 0x82809430 (attrib-sdk wave 2026-07-27)
// ============================================================================
// In-place bucket construction: store the key and payload word, resolve the
// stored type INDEX from the 64-bit type key through the attribute database's
// type registry (GetTypeDesc falls back to the NULL type when the key is
// unregistered), raise the occupied bit over the serialised flag byte, and --
// for laid-out/inherited payloads under a live base -- rebase the payload word
// (value -= base, the X360's offset-relative store).
Attrib::HashMap::Node::Node(u64 luKey, u64 luTypeKey, void* lpValue,
                            bool lbLaidOut, u8 lu8Flags, void* lpBase)
{
    mKey = luKey;
    mpValue = lpValue;
    mTypeIndex = static_cast<u16>(Database::Get().GetTypeDesc(luTypeKey).mIndex);
    // (mu8SearchLen untouched -- the X360 ctor leaves the slot's cached run length)
    mFlags = static_cast<u8>(lu8Flags | 0x80);
    if (lbLaidOut && ((lu8Flags & 0x10) != 0 || (lu8Flags & 0x20) != 0))
    {
        mpValue = reinterpret_cast<void*>(
            reinterpret_cast<uintptr_t>(lpValue) - reinterpret_cast<uintptr_t>(lpBase));
    }
}

// ============================================================================
// Attrib::HashMap::CountSearchCacheLines @ 0x828048C8
// ============================================================================
// Replay the probe run a lookup of luKey would walk and count how many DISTINCT
// cache lines it touches, where a cache line is (bucketAddress >> lu8CacheLineShift).
// Store-for-store from the X360: an empty table, or a zero key, touches nothing.
int Attrib::HashMap::CountSearchCacheLines(u64 luKey, u8 lu8CacheLineShift) const
{
    int liCacheLines = 0;

    if (muCount == 0 || luKey == 0)
        return 0;

    const u8 lu8KeyShift = mu8KeyShift;
    const u32 luCapacity = muCapacity;
    Bucket* lpBuckets = mpBuckets;

    // Home index = rotl32(key, keyShift) % capacity.
    const u32 luHash = static_cast<u32>((luKey >> (64 - lu8KeyShift)) |
                                        (luKey << lu8KeyShift));
    u32 luIndex = luHash % luCapacity;

    Bucket* lpBucket = &lpBuckets[luIndex];
    const u8 lu8SearchLen = lpBucket->mu8SearchLen;

    // Seed the cache-line accumulator from the home bucket's address.
    u32 luPrevCacheLine = reinterpret_cast<uintptr_t>(lpBucket) >> lu8CacheLineShift;
    if (luPrevCacheLine != 0)
        liCacheLines = 1;

    if (lu8SearchLen != 0)
    {
        u32 luStep = 0;
        do
        {
            Bucket* lpProbe = &lpBuckets[luIndex];
            if (lpProbe->Key() == luKey)
                break;

            luIndex = (luIndex + 1) % luCapacity;
            Bucket* lpNext = &lpBuckets[luIndex];
            const u32 luCacheLine =
                reinterpret_cast<uintptr_t>(lpNext) >> lu8CacheLineShift;
            if (luCacheLine != luPrevCacheLine)
            {
                luPrevCacheLine = luCacheLine;
                ++liCacheLines;
            }
            ++luStep;
        }
        while (luStep < lu8SearchLen);
    }

    return liCacheLines;
}

// ============================================================================
// Attrib::HashMap::Find @ 0x82804838
// ============================================================================
// FindIndex, then resolve to the live Bucket* (NULL when the index is out of range
// or the bucket is free).
Attrib::HashMap::Bucket* Attrib::HashMap::Find(u64 luKey) const
{
    const unsigned int luIndex = FindIndex(luKey);

    bool lbValid = true;
    if (luIndex >= muCapacity)
    {
        lbValid = false;
    }
    else
    {
        Bucket* lpBucket = &mpBuckets[luIndex];
        if (!lpBucket->IsOccupied())
            lbValid = false;
    }

    if (lbValid)
        return &mpBuckets[luIndex];
    return NULL;
}

// ============================================================================
// Attrib::HashMap::FindIndex @ 0x82804700
// ============================================================================
// Open-addressing lookup. Home the key by rotate-hash, then walk its probe run.
// Returns the bucket index the key lives at, or muCapacity when absent (or empty).
// The X360's Hex-Rays modulus is a decompiler artefact; the asm's divwu divisor is
// muCapacity (lhz r9,4).
unsigned int Attrib::HashMap::FindIndex(u64 luKey) const
{
    if (muCount == 0)
        return muCapacity;

    const u8 lu8KeyShift = mu8KeyShift;
    const u32 luCapacity = muCapacity;
    Bucket* lpBuckets = mpBuckets;

    // Home index = rotl32(key, keyShift) % capacity.
    const u32 luHash = static_cast<u32>((luKey >> (64 - lu8KeyShift)) |
                                        (luKey << lu8KeyShift));
    u32 luIndex = luHash % luCapacity;

    Bucket* lpHome = &lpBuckets[luIndex];
    const u8 lu8SearchLen = lpHome->mu8SearchLen;

    if (lu8SearchLen != 0)
    {
        u32 luStep = 0;
        do
        {
            Bucket* lpProbe = &lpBuckets[luIndex];
            if (lpProbe->Key() == luKey)
                break;

            CGS_ASSERT(lpProbe->IsOccupied(), "Table invariant is broken.");

            ++luStep;
            luIndex = (luIndex + 1) % luCapacity;
        }
        while (luStep < lu8SearchLen);
    }

    Bucket* lpFinal = &lpBuckets[luIndex];
    if (lpFinal->Key() != luKey)
        return muCapacity;
    return luIndex;
}

// ============================================================================
// Attrib::HashMap::GetKeyAtIndex @ 0x82802660
// ============================================================================
// Return the key stored at a bucket index, asserting the index maps to a live
// bucket first. A free bucket (flag bit clear) reads back key 0.
u64 Attrib::HashMap::GetKeyAtIndex(u32 luIndex) const
{
    bool lbValid = true;
    if (luIndex >= muCapacity)
    {
        lbValid = false;
    }
    else
    {
        Bucket* lpBucket = &mpBuckets[luIndex];
        if (!lpBucket->IsOccupied())
            lbValid = false;
    }

    CGS_ASSERT(lbValid, "Attrib::HashMap invalid index used.");

    Bucket* lpBucket = &mpBuckets[luIndex];
    if (!lpBucket->IsOccupied())
        return 0;
    return lpBucket->mKey;
}

// ============================================================================
// Attrib::HashMap::PreFlightAdd @ 0x828027A8
// ============================================================================
// From a home index, walk the probe run for luKey. If the key already lives in the
// table, return -1 (0xFFFFFFFF). Otherwise return the index of the first FREE bucket
// the run reaches -- where the key could be inserted -- and write the number of probe
// steps taken into *lpiSteps.
unsigned int Attrib::HashMap::PreFlightAdd(u64 luKey, unsigned int luStartIndex, int* lpiSteps)
{
    u32 luIndex = luStartIndex;
    *lpiSteps = 0;

    Bucket* lpBucket = &mpBuckets[luIndex];
    if (!lpBucket->IsOccupied())
        return luIndex;

    while (true)
    {
        if (lpBucket->Key() == luKey)
            return 0xFFFFFFFFu;

        const u32 luNext = luIndex + 1;
        const u32 luCapacity = muCapacity;
        *lpiSteps = *lpiSteps + 1;
        luIndex = luNext % luCapacity;

        lpBucket = &mpBuckets[luIndex];
        if (!lpBucket->IsOccupied())
            return luIndex;
    }
}

// ============================================================================
// Attrib::HashMap::Release @ 0x82802718
// ============================================================================
// Drop one shared reference on the hash-map. When the live refcount is already at
// most one this is the final release: assert it was not already zero, zero the
// refcount, and return whether it had been exactly 1 (the "was actually held"
// result the callers thread back as an int). While more than one reference is
// held, decrement and report "not released" (0). DWARF marks this const, but the
// X360 body writes mRefCount, so the reconstruction takes a non-const this.
bool Attrib::HashMap::Release() const
{
    // ⚠️ FIXED 2026-08-01 (Prepare wave): this used to reach the refcount as
    // `*(u16*)((u8*)this + 8)` -- the CONSOLE offset. mpBuckets is a POINTER, so on x64
    // muCapacity sits at +8 and muRefCount at +12: every Release was decrementing the
    // BUCKET COUNT of the table it was supposed to be releasing, while every AddRef
    // (Collection::AddRef / Instance::Instance / RefSpec::RefSpec, all of which already
    // used the named member) bumped the real muRefCount. Symptom: "Too many releases of
    // object!" on the first Instance destructor that ever ran against a real collection --
    // i.e. the moment DirectorResourceManager::Prepare landed. Read it BY NAME.
    u16* lpuRefCount = &const_cast<Attrib::HashMap*>(this)->muRefCount;
    const unsigned int luRefs = *lpuRefCount;

    bool lbReleased;
    u16 lu16NewCount;
    if (luRefs <= 1)
    {
        lbReleased = (luRefs == 1);
        CGS_ASSERT(*lpuRefCount != 0, "Too many releases of object!\n");
        lu16NewCount = 0;
    }
    else
    {
        lbReleased = false;
        lu16NewCount = static_cast<u16>(luRefs - 1);
    }
    *lpuRefCount = lu16NewCount;
    return lbReleased;
}

// ============================================================================
// Attrib::HashMap::Remove @ 0x82807A78
// ============================================================================
// Remove one node from the open-addressed table and return the address the node's
// payload resolves to (the raw pointer for a by-value/laid-out/inherited attribute,
// so the caller can release it). The node is asserted to be a live slot inside this
// table. Its payload slot is invalidated (key and flags cleared, back-reference
// re-homed to the node itself) and mNumEntries is dropped. When lbRestore is false
// only the node's search length is zeroed; when true the table's search-length
// invariant is repaired by walking UpdateSearchLength from the vacated home index
// until it reports a settled table.
void* Attrib::HashMap::Remove(Node* lpNode, void* lpBase, const Collection* lpCollection, bool lbRestore)
{
    u8* lpThis = reinterpret_cast<u8*>(this);
    u8* lpNodeBytes = reinterpret_cast<u8*>(lpNode);
    Node* lpTable = *reinterpret_cast<Node**>(lpThis);

    // The node must be a valid slot inside this table's bucket array.
    const u8 lu8Flags0 = *reinterpret_cast<u8*>(lpNodeBytes + 0xF);
    const u16 lu16TableSize = *reinterpret_cast<u16*>(lpThis + 4);
    u8* lpTableEnd = reinterpret_cast<u8*>(lpTable) + (static_cast<u32>(lu16TableSize) << 4);
    CGS_ASSERT((lu8Flags0 & 0x80) != 0
                   && lpNodeBytes >= reinterpret_cast<u8*>(lpTable)
                   && lpNodeBytes < lpTableEnd,
               "Attrib::HashMap removing node which is not valid node in table.");

    // The node's stored value (its raw payload word) drives the home-index rehash.
    const u8 lu8Flags = *reinterpret_cast<u8*>(lpNodeBytes + 0xF);
    const u64 lu64Value = (lu8Flags & 0x80) != 0 ? *reinterpret_cast<u64*>(lpNodeBytes) : 0;

    // Resolve the address the caller must act on, per the payload's storage flags.
    u8* lpResult;
    if ((lu8Flags & 0x40) != 0)
    {
        // By-value: the payload lives inline in the node (node+8).
        lpResult = lpNodeBytes + 8;
    }
    else if ((lu8Flags & 0x10) != 0)
    {
        // Laid out: an offset into the caller-supplied data base.
        lpResult = reinterpret_cast<u8*>(lpBase) + *reinterpret_cast<u32*>(lpNodeBytes + 8);
    }
    else if ((lu8Flags & 0x20) != 0)
    {
        // Inherited: an offset into the parent collection's data area.
        u8* lpParent = *reinterpret_cast<u8* const*>(
            reinterpret_cast<const u8*>(lpCollection) + 0x18); // Collection.mpSubCollection
        u8* lpParentData = *reinterpret_cast<u8**>(lpParent + 8);
        const u32 luDataOfs = *reinterpret_cast<u32*>(lpParentData + 0x34);
        lpResult = reinterpret_cast<u8*>(static_cast<uintptr_t>(luDataOfs)
                                         + *reinterpret_cast<u32*>(lpNodeBytes + 8));
    }
    else
    {
        // Plain pointer stored inline.
        lpResult = *reinterpret_cast<u8**>(lpNodeBytes + 8);
    }

    // Re-home the node's back-reference to itself and invalidate the slot.
    *reinterpret_cast<Node**>(lpNodeBytes + 8) = lpNode;
    *reinterpret_cast<u64*>(lpNodeBytes) = 0;
    *reinterpret_cast<u8*>(lpNodeBytes + 0xF) = 0;
    --*reinterpret_cast<u16*>(lpThis + 6);

    if (!lbRestore)
    {
        *reinterpret_cast<u8*>(lpNodeBytes + 0xE) = 0; // mMax
        return lpResult;
    }

    // Repair the search-length invariant from the vacated home bucket outward.
    const u8 lu8KeyShift = *reinterpret_cast<u8*>(lpThis + 0xB);
    const unsigned int luTableSize = *reinterpret_cast<u16*>(lpThis + 4);
    const unsigned int luNodeIndex =
        static_cast<unsigned int>((lpNodeBytes - reinterpret_cast<u8*>(lpTable)) >> 4);
    const u64 lu64Rot = (lu64Value >> (64 - lu8KeyShift)) | (lu64Value << lu8KeyShift);
    const unsigned int luHome = static_cast<unsigned int>(lu64Rot % luTableSize);

    unsigned int luUpdated = UpdateSearchLength(luHome, luNodeIndex);
    if (luUpdated < *reinterpret_cast<u16*>(lpThis + 4))
    {
        do
            luUpdated = UpdateSearchLength(luUpdated, luUpdated);
        while (luUpdated < *reinterpret_cast<u16*>(lpThis + 4));
    }
    return lpResult;
}

// ============================================================================
// Attrib::HashMap::Transfer @ 0x828049D8
// ============================================================================
// Move one live node from an old bucket buffer into this (freshly grown) table --
// the per-node worker RebuildTable drives while re-homing every entry. Finds the
// destination slot via PreFlightAdd (which also reports how many probes the insert
// cost in luProbeCount), copies the node's payload (key/value/type/flags) into that
// slot, invalidates the source node, then folds the probe cost back into the home
// bucket's max-search length and the table's worst-collision high-water mark.
void Attrib::HashMap::Transfer(Node& lrNode)
{
    // (Named-member x64 form of the raw X360 offset walk -- the earlier
    // byte-offset transcription addressed the X360 container/16-byte-node
    // layout, wrong once the pointers widened.)
    CGS_ASSERT(muCount < muCapacity,
               "Attrib::HashMap number of entries is not valid during Transfer.");

    const u64 lu64Key = lrNode.IsOccupied() ? lrNode.mKey : 0;
    const u8 lu8KeyShift = mu8KeyShift;
    const unsigned int luTableSize = muCapacity;

    const u64 lu64Rot = (lu64Key >> (64 - lu8KeyShift)) | (lu64Key << lu8KeyShift);
    const unsigned int luHome = static_cast<unsigned int>(lu64Rot % luTableSize);

    int liProbeCount = 0;
    const unsigned int luDest = PreFlightAdd(lu64Key, luHome, &liProbeCount);
    const unsigned int luProbeCount = static_cast<unsigned int>(liProbeCount);

    CGS_ASSERT(luDest < luTableSize, "Attrib::HashMap Transfer to invalid index.");

    // Copy the source node's payload into the destination slot.
    Node* lpDest = &mpBuckets[luDest];
    const u8 lu8ProbeByte = static_cast<u8>(luProbeCount);
    lpDest->mKey = lrNode.mKey;
    lpDest->mTypeIndex = lrNode.mTypeIndex;
    lpDest->mpValue = lrNode.mpValue;
    lpDest->mFlags = lrNode.mFlags;

    // Invalidate the source node (value re-homed to itself, key/flags cleared).
    lrNode.mpValue = &lrNode;
    lrNode.mFlags = 0;
    lrNode.mKey = 0;

    // Fold the probe cost into the home bucket's max-search and the worst-collision mark.
    Node* lpHome = &mpBuckets[luHome];
    if (lpHome->mu8SearchLen <= lu8ProbeByte)
        lpHome->mu8SearchLen = lu8ProbeByte;
    if (luProbeCount > mu8MaxSearchLength)
        mu8MaxSearchLength = lu8ProbeByte;

    ++muCount;
}

// ============================================================================
// Attrib::HashMap::UpdateSearchLength @ 0x82804B50
// ============================================================================
// Repair the open-addressing search-length bookkeeping after a slot is vacated.
// Given the home index whose chain was disturbed (luIndex) and the just-freed slot
// (luFreedIndex), it finds the last live entry still homing to luIndex, back-shifts
// it into the freed slot, recomputes that home bucket's max-probe length, and --
// when that bucket held the table-wide worst collision -- rescans every bucket to
// re-derive the worst-collision high-water mark. Returns the index that was shifted
// (fed back in the Remove repair loop), or -1u when no further shift is required.
unsigned int Attrib::HashMap::UpdateSearchLength(unsigned int luIndex, unsigned int luFreedIndex)
{
    u8* lpThis = reinterpret_cast<u8*>(this);
    Node* lpTable = *reinterpret_cast<Node**>(lpThis);
    const unsigned int luTableSize = *reinterpret_cast<u16*>(lpThis + 4);
    const u8 lu8KeyShift = *reinterpret_cast<u8*>(lpThis + 0xB);
    (void)luTableSize;
    (void)lu8KeyShift;

    unsigned int luHome = luIndex;
    if (luIndex == luFreedIndex)
    {
        u8* lpFreedSlot = reinterpret_cast<u8*>(lpTable) + (luFreedIndex << 4);
        if (*reinterpret_cast<u8*>(lpFreedSlot + 0xE) == 0) // mMax
        {
            // Walk backwards from the freed slot to find the entry to back-shift.
            unsigned int luWorst = *reinterpret_cast<u8*>(lpThis + 0xA); // mWorstCollision
            luHome = (*reinterpret_cast<u16*>(lpThis + 4) - luWorst + luFreedIndex) % *reinterpret_cast<u16*>(lpThis + 4);
            u8* lpProbe = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis)) + (luHome << 4);
            if (*reinterpret_cast<u8*>(lpProbe + 0xE) < luWorst)
            {
                while (luWorst != 0)
                {
                    Node* lpBase = *reinterpret_cast<Node**>(lpThis);
                    --luWorst;
                    luHome = (luHome + 1) % *reinterpret_cast<u16*>(lpThis + 4);
                    if (*reinterpret_cast<u8*>(reinterpret_cast<u8*>(lpBase) + (luHome << 4) + 0xE)
                            >= luWorst)
                        break;
                }
            }
            if (luWorst == 0)
                return static_cast<unsigned int>(-1);
        }
        else
        {
            luHome = luFreedIndex;
        }
    }

    // The disturbed home bucket's recorded max-probe length.
    const unsigned int luMax =
        *reinterpret_cast<u8*>(reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis))
                               + (luHome << 4) + 0xE);
    CGS_ASSERT(luMax <= *reinterpret_cast<u8*>(lpThis + 0xA), "Error in table invariant.");

    // The tail slot of the chain -- the entry that will be shifted down.
    const unsigned int luTail =
        (luMax + luHome) % *reinterpret_cast<u16*>(lpThis + 4);
    u8* lpTailSlot = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis)) + (luTail << 4);
    if ((*reinterpret_cast<u8*>(lpTailSlot + 0xF) & 0x80) != 0)
    {
        const u64 lu64Value = *reinterpret_cast<u64*>(lpTailSlot);
        const u8 lu8Shift = *reinterpret_cast<u8*>(lpThis + 0xB);
        const u64 lu64Rot = (lu64Value >> (64 - lu8Shift)) | (lu64Value << lu8Shift);
        CGS_ASSERT(luHome == static_cast<unsigned int>(lu64Rot % *reinterpret_cast<u16*>(lpThis + 4)),
                   "Incorrect max search length found in table.");
    }

    u8* lpFreed = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis)) + (luFreedIndex << 4);
    CGS_ASSERT((*reinterpret_cast<u8*>(lpFreed + 0xF) & 0x80) == 0, "Free node is not invalid!");

    // Back-shift the tail entry into the freed slot.
    if (luFreedIndex != luTail)
    {
        Node* lpBase = *reinterpret_cast<Node**>(lpThis);
        u8* lpSrc = reinterpret_cast<u8*>(lpBase) + (luTail << 4);
        u8* lpDst = reinterpret_cast<u8*>(lpBase) + (luFreedIndex << 4);
        *reinterpret_cast<u64*>(lpDst) = *reinterpret_cast<u64*>(lpSrc);            // key
        *reinterpret_cast<u16*>(lpDst + 0xC) = *reinterpret_cast<u16*>(lpSrc + 0xC); // mTypeIndex
        *reinterpret_cast<u32*>(lpDst + 8) = *reinterpret_cast<u32*>(lpSrc + 8);     // value
        *reinterpret_cast<u8*>(lpDst + 0xF) = *reinterpret_cast<u8*>(lpSrc + 0xF);   // mFlags
        *reinterpret_cast<u8**>(lpSrc + 8) = lpSrc;
        *reinterpret_cast<u8*>(lpSrc + 0xF) = 0;
        *reinterpret_cast<u64*>(lpSrc) = 0;
    }

    u8* lpTailAfter = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis)) + (luTail << 4);
    CGS_ASSERT((*reinterpret_cast<u8*>(lpTailAfter + 0xF) & 0x80) == 0, "Freed node is not invalid!");

    // Recompute the home bucket's max-probe length over the remaining chain.
    unsigned int luNewMax = 0;
    if (luMax > 1)
    {
        for (unsigned int luProbe = 1; luProbe < luMax; ++luProbe)
        {
            const unsigned int luProbeIndex =
                (luProbe + luHome) % *reinterpret_cast<u16*>(lpThis + 4);
            u8* lpSlot = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis)) + (luProbeIndex << 4);
            const u64 lu64V = (*reinterpret_cast<u8*>(lpSlot + 0xF) & 0x80) != 0
                                  ? *reinterpret_cast<u64*>(lpSlot) : 0;
            const u8 lu8Shift = *reinterpret_cast<u8*>(lpThis + 0xB);
            const u64 lu64Rot = (lu64V << lu8Shift) | (lu64V >> (64 - lu8Shift));
            if (static_cast<unsigned int>(lu64Rot % *reinterpret_cast<u16*>(lpThis + 4)) == luHome)
                luNewMax = luProbe;
        }
    }

    CGS_ASSERT(luNewMax <= *reinterpret_cast<u8*>(lpThis + 0xA),
               "Worst search got worse after making an improvement!");
    *reinterpret_cast<u8*>(reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis))
                          + (luHome << 4) + 0xE) = static_cast<u8>(luNewMax);

    // If this bucket previously held the table-wide worst collision and it has now
    // improved, rescan every bucket to re-derive the worst-collision high-water mark.
    const unsigned int luWorst = *reinterpret_cast<u8*>(lpThis + 0xA);
    if (luMax == luWorst)
    {
        u8* lpBase = reinterpret_cast<u8*>(*reinterpret_cast<Node**>(lpThis));
        if (*reinterpret_cast<u8*>(lpBase + (luFreedIndex << 4) + 0xE) < luWorst && luNewMax < luMax)
        {
            *reinterpret_cast<u8*>(lpThis + 0xA) = 0;
            const unsigned int luCount = *reinterpret_cast<u16*>(lpThis + 4);
            if (luCount != 0)
            {
                u8* lpScan = lpBase + 0xE;
                unsigned int luScanIdx = 0;
                do
                {
                    const unsigned int luCur = *reinterpret_cast<u8*>(lpThis + 0xA);
                    if (luCur >= luWorst)
                        break;
                    if (*lpScan > luCur)
                        *reinterpret_cast<u8*>(lpThis + 0xA) = *lpScan;
                    ++luScanIdx;
                    lpScan += 16;
                }
                while (luScanIdx < *reinterpret_cast<u16*>(lpThis + 4));
            }
        }
    }

    return luTail;
}
