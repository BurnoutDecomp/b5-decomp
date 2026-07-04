#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"   // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator

// AttribSys runtime -- the two live VecHashMap instantiations off the X360 spine:
//   VecHashMap<Attrib::Key, Attrib::Class,      Attrib::Class::TablePolicy, false, 16u>
//   VecHashMap<Attrib::Key, Attrib::Collection, Attrib::Class::TablePolicy, true,  96u>
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (CgsAttribSysUnity TU).
//
// Node stride is 16 bytes throughout (X360 slwi ,4 / addi ,0x10). A bucket is FREE/INVALID
// iff its value pointer points at the node itself (the X360 self-pointer sentinel). The
// divwu/mullw/subf + twllei guard pairs are the compiler's guarded unsigned modulo,
// reproduced here as plain %.

namespace Attrib
{
    // ---- statics ------------------------------------------------------------
    // The per-TU freed-byte census the inlined TablePolicy::Free bumps on release
    // (X360 dword_83011BA4). Zero-initialised at load; defined here exactly once.
    u32 gTablePolicyFreedBytes = 0;

    // ---- cross-TU trap stubs (own ledger TUs) -------------------------------
    // Attrib::TableFreeFunc (TablePolicy::Free thunk) -- routes a bucket array back to the
    // AttribSys package allocator + bumps the hash-map byte census. Own TU; trap stub here.
    void* TableFreeFunc(void* lpBlock, size_t liSize)
    {
        (void)lpBlock;
        (void)liSize;
        __debugbreak();
        return NULL;
    }

    // Attrib::Collection scalar deleting destructor -- own TU; trap stub here.
    void* Collection_ScalarDeletingDtor(Collection* lpCollection, int liDeleteFlag)
    {
        (void)lpCollection;
        (void)liDeleteFlag;
        __debugbreak();
        return NULL;
    }
}

// ============================================================================
// VecHashMap<Attrib::Key, Attrib::Class, Attrib::Class::TablePolicy, false, 16u>
// ============================================================================

// @ 0x8280AAE8 -- public insert. Delegate to InternalAdd, then (if the worst-case
// collision run has grown past the rehash threshold of 16) grow-and-rehash the table.
// The InternalAdd result is what Add returns even when a subsequent rehash runs.
bool VecHashMap_Attrib_Class_TablePolicy_0_16::Add(u64 luKey, Attrib::Class* lpValue)
{
    const bool lbAdded = InternalAdd(luKey, lpValue);

    if (muWorstCollision > 16u)
    {
        CGS_ASSERT(muFixedAlloc == 0, "Attempted to grow table with fixed allocation");

        u32 luNewCount;
        if (muTableSize != 0)
            luNewCount = ((20u * muTableSize) / 16u + 3u) & 0xFFFFFFFCu;
        else
            luNewCount = 1u;

        RebuildTable(luNewCount);
    }

    return lbAdded;
}

// @ 0x828063C0 -- open-addressing lookup. Returns the bucket index holding luKey, or
// muTableSize when the key is absent. muMax on the home bucket bounds the probe run.
unsigned int VecHashMap_Attrib_Class_TablePolicy_0_16::FindIndex(u64 luKey) const
{
    if (muNumEntries == 0)
        return muTableSize;

    const unsigned int luSize = muTableSize;
    Node* const lpTable = mpTable;
    unsigned int luProbes = 0;

    unsigned int luIndex = static_cast<unsigned int>(luKey % luSize);
    const unsigned int luMax = lpTable[luIndex].muMax;
    if (luMax != 0)
    {
        do
        {
            Node* lpNode = &lpTable[luIndex];
            const u64 luNodeKey =
                (lpNode->mpPtr == reinterpret_cast<Attrib::Class*>(lpNode)) ? 0 : lpNode->mKey;
            if (luNodeKey == luKey)
                break;

            CGS_ASSERT(lpNode->mpPtr != reinterpret_cast<Attrib::Class*>(lpNode),
                       "Invalid node found during search; table invariant is broken");

            ++luProbes;
            luIndex = (luIndex + 1) % luSize;
        }
        while (luProbes < luMax);
    }

    Node* lpNode = &lpTable[luIndex];
    const u64 luNodeKey =
        (lpNode->mpPtr == reinterpret_cast<Attrib::Class*>(lpNode)) ? 0 : lpNode->mKey;
    if (luNodeKey != luKey)
        return muTableSize;
    return luIndex;
}

// @ 0x828064D0 -- Find(key): resolve the slot with FindIndex, then return the stored
// Class* when the slot is in range and holds a live (non-sentinel) node; otherwise NULL.
Attrib::Class* VecHashMap_Attrib_Class_TablePolicy_0_16::Find(u64 luKey) const
{
    const u32 luIndex = FindIndex(luKey);

    if (luIndex >= muTableSize)
        return NULL;

    Node& lrNode = mpTable[luIndex];
    if (!lrNode.IsValid())
        return NULL;

    return lrNode.Get();
}

// @ 0x82807430 -- GetKeyAtIndex(index): assert the index is valid (in range AND a live,
// non-sentinel node), then return that node's key (Node::Key() yields 0 for a sentinel).
u64 VecHashMap_Attrib_Class_TablePolicy_0_16::GetKeyAtIndex(u32 luIndex) const
{
    CGS_ASSERT(ValidIndex(luIndex), "GetKeyAtIndex passed invalid index.");

    return mpTable[luIndex].Key();
}

// @ 0x82808BB0 -- core open-addressing insert (grow-on-worst-collision is Add's job).
// Grows the table only when it is exactly full. Returns false if luKey is already
// present (no insert), true on a fresh insert. Includes the home-bucket muMax high-water
// update and the muWorstCollision census.
bool VecHashMap_Attrib_Class_TablePolicy_0_16::InternalAdd(u64 luKey, Attrib::Class* lpValue)
{
    // Grow if the table is completely full.
    if (muNumEntries == muTableSize)
    {
        CGS_ASSERT(muFixedAlloc == 0, "Attempted to add to table with fixed allocation");

        u32 luNewCount;
        if (muTableSize != 0)
            luNewCount = ((20u * muTableSize) / 16u + 3u) & 0xFFFFFFFCu;
        else
            luNewCount = 1u;

        RebuildTable(luNewCount);
    }

    const unsigned int luSize = muTableSize;
    Node* const lpTable = mpTable;
    unsigned int luProbes = 0;

    unsigned int luIndex = static_cast<unsigned int>(luKey % luSize);

    // Walk the probe chain unless the home bucket is already free.
    if (lpTable[luIndex].mpPtr != reinterpret_cast<Attrib::Class*>(&lpTable[luIndex]))
    {
        for (;;)
        {
            Node* lpNode = &lpTable[luIndex];
            const u64 luNodeKey =
                (lpNode->mpPtr == reinterpret_cast<Attrib::Class*>(lpNode)) ? 0 : lpNode->mKey;
            if (luNodeKey == luKey)
                return false; // key already present

            ++luProbes;
            luIndex = (luIndex + 1) % luSize;
            if (lpTable[luIndex].mpPtr == reinterpret_cast<Attrib::Class*>(&lpTable[luIndex]))
                break; // found a free slot
        }
    }

    // Insert into the free bucket at luIndex.
    Node* lpSlot = &lpTable[luIndex];
    if (lpSlot != NULL)
    {
        lpSlot->mKey  = luKey;
        lpSlot->mpPtr = lpValue;
    }

    // Bump the home bucket's max-search length.
    Node* lpHome = &mpTable[static_cast<unsigned int>(luKey % luSize)];
    if (lpHome->muMax < luProbes)
        lpHome->muMax = luProbes;

    if (luProbes > muWorstCollision)
        muWorstCollision = static_cast<u16>(luProbes);

    ++muNumEntries;
    return true;
}

// @ 0x82806D60 -- UpdateSearchLength: called after a removal frees the bucket at
// luFreeIndex whose home bucket is luOldIndex. It pulls the last entry of the home
// bucket's probe run down into the hole, recomputes that home bucket's muMax, and (when
// the removed run was the table's worst) rescans to recompute muWorstCollision. Returns
// the bucket the moved entry now occupies, or -1u when there was nothing to compact.
//
// Reconstructed store-for-store from the X360; the backward-scan block mirrors the
// asm goto graph (E_COMPACT == LABEL_4; the worst==0 -> return -1 gate is unconditional
// at the scan's join, as in the X360 loc_82806E90).
unsigned int VecHashMap_Attrib_Class_TablePolicy_0_16::UpdateSearchLength(unsigned int luOldIndex,
                                                                         unsigned int luFreeIndex)
{
    unsigned int luIndex = luOldIndex;

    if (luOldIndex == luFreeIndex)
    {
        if (mpTable[luFreeIndex].muMax == 0)
        {
            // Walk back from the far end of the worst run looking for the last home
            // bucket whose cached run still reaches the freed slot.
            const unsigned int luSize  = muTableSize;
            unsigned int       luWorst = muWorstCollision;
            luIndex = (luSize - luWorst + luFreeIndex) % luSize;

            if (mpTable[luIndex].muMax < luWorst)
            {
                if (luWorst == 0)
                    return static_cast<unsigned int>(-1);
                for (;;)
                {
                    --luWorst;
                    luIndex = (luIndex + 1) % luSize;
                    if (mpTable[luIndex].muMax >= luWorst)
                        break; // -> join
                    if (luWorst == 0)
                        return static_cast<unsigned int>(-1);
                }
            }

            // Join (X360 loc_82806E90): a zero worst here means nothing to compact.
            if (luWorst == 0)
                return static_cast<unsigned int>(-1);
        }
        else
        {
            luIndex = luFreeIndex;
        }
    }

    // LABEL_4: compact.
    const unsigned int luHomeIndex = luIndex;
    const unsigned int luRun = mpTable[luIndex].muMax;

    CGS_ASSERT(luRun <= muWorstCollision, "Error in table invariant.");

    const unsigned int luSize = muTableSize;
    const unsigned int luTarget = (luRun + luIndex) % luSize;
    Node* lpTargetNode = &mpTable[luTarget];
    if (lpTargetNode->mpPtr != reinterpret_cast<Attrib::Class*>(lpTargetNode))
    {
        CGS_ASSERT(luIndex == static_cast<unsigned int>(lpTargetNode->mKey % luSize),
                   "Incorrect max search length found in table.");
    }

    Node* lpFreeNode = &mpTable[luFreeIndex];
    CGS_ASSERT(lpFreeNode->mpPtr == reinterpret_cast<Attrib::Class*>(lpFreeNode),
               "Free node is not invalid!");

    if (luFreeIndex != luTarget)
    {
        Node* lpSrc = &mpTable[luTarget];
        Node* lpDst = &mpTable[luFreeIndex];
        lpDst->mKey  = lpSrc->mKey;
        lpDst->mpPtr = lpSrc->mpPtr;
        lpSrc->mKey  = 0;
        lpSrc->mpPtr = reinterpret_cast<Attrib::Class*>(lpSrc); // invalidate
    }

    Node* lpMovedFrom = &mpTable[luTarget];
    CGS_ASSERT(lpMovedFrom->mpPtr == reinterpret_cast<Attrib::Class*>(lpMovedFrom),
               "Freed node is not invalid!");

    // Recompute the home bucket's max-search length over the surviving run.
    unsigned int luNewMax = 0;
    for (unsigned int luProbe = 1; luProbe < luRun; ++luProbe)
    {
        Node* lpNode = &mpTable[(luProbe + luIndex) % luSize];
        const u64 luNodeKey =
            (lpNode->mpPtr == reinterpret_cast<Attrib::Class*>(lpNode)) ? 0 : lpNode->mKey;
        if (static_cast<unsigned int>(luNodeKey % luSize) == luIndex)
            luNewMax = luProbe;
    }

    CGS_ASSERT(luNewMax <= muWorstCollision,
               "Worst search got worse after making an improvement!");

    mpTable[luHomeIndex].muMax = luNewMax;

    // If we just shortened the table's worst run, rescan to recompute muWorstCollision.
    const unsigned int luOldWorst = muWorstCollision;
    if (luRun == luOldWorst)
    {
        if (mpTable[luFreeIndex].muMax < luOldWorst && luNewMax < luRun)
        {
            const unsigned int luCount = muTableSize;
            muWorstCollision = 0;
            if (luCount != 0)
            {
                Node* lpNode = mpTable;
                unsigned int luScanned = 0;
                do
                {
                    if (muWorstCollision >= luOldWorst)
                        break;
                    if (lpNode->muMax > muWorstCollision)
                        muWorstCollision = static_cast<u16>(lpNode->muMax);
                    ++luScanned;
                    ++lpNode;
                }
                while (luScanned < muTableSize);
            }
        }
    }

    return luTarget;
}

// @ 0x82808D30 -- reinitialise the (already-allocated) new bucket array to all-invalid,
// then re-insert every live entry from the old bucket array via InternalAdd, and (when
// asked) hand the old array back to the AttribSys allocator. A node is free/invalid iff
// its mpPtr points at the node itself (the X360 self-pointer sentinel).
void VecHashMap_Attrib_Class_TablePolicy_0_16::CopyFromOldTable(Node* lpOldTable,
                                                               unsigned int luOldSize,
                                                               bool lbFree)
{
    // Invalidate every node of the freshly-allocated table.
    if (muTableSize != 0)
    {
        unsigned int luIndex = 0;
        do
        {
            Node* lpNode = &mpTable[luIndex];
            if (lpNode != NULL)
            {
                lpNode->mKey  = 0;
                lpNode->mpPtr = reinterpret_cast<Attrib::Class*>(lpNode); // self => invalid
                lpNode->muMax = 0;
            }
            ++luIndex;
        }
        while (luIndex < muTableSize);
    }

    if (lpOldTable != NULL)
    {
        // Re-insert every live old entry.
        if (luOldSize != 0)
        {
            Node* lpNode = lpOldTable;
            unsigned int luRemaining = luOldSize;
            do
            {
                if (reinterpret_cast<Attrib::Class*>(lpNode) != lpNode->mpPtr)
                    InternalAdd(lpNode->mKey, lpNode->mpPtr);
                --luRemaining;
                ++lpNode;
            }
            while (luRemaining != 0);
        }

        // Release the old bucket array.
        if (lbFree)
        {
            CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
                CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
            lpAllocator->Free(lpOldTable, luOldSize * sizeof(Node));
        }
    }
}

// @ 0x82806928 -- (re)size the bucket array. luNewCount==0 means 'release when empty':
// if the table holds nothing and is not a fixed allocation, free the array and null it.
// Otherwise allocate a fresh array of at least luNewCount buckets, rehash the old
// contents into it (CopyFromOldTable frees the old array), and keep growing by one bucket
// per pass until the worst-case collision run drops back to the 16-slot threshold.
// GetAttribSysAllocator() carries the sbHasLinearAllocator assert the X360 inlines here.
void VecHashMap_Attrib_Class_TablePolicy_0_16::RebuildTable(unsigned int luNewCount)
{
    if (luNewCount == 0)
    {
        if (muFixedAlloc == 0 && muNumEntries == 0 && mpTable != NULL)
        {
            Attrib::TableFreeFunc(mpTable, muTableSize * sizeof(Node));
            mpTable = NULL;
        }
        return;
    }

    u16 luSize = static_cast<u16>(luNewCount - 1);
    do
    {
        ++luSize;
        const unsigned int luOldSize = muTableSize;
        muNumEntries     = 0;
        muWorstCollision = 0;
        Node* lpOldTable = mpTable;
        muTableSize      = luSize;

        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        mpTable = static_cast<Node*>(
            lpAllocator->Malloc((luSize * sizeof(Node)) & 0xFFFF0u, 0));

        CopyFromOldTable(lpOldTable, luOldSize, true);
    }
    while (muWorstCollision > 16u);
}

// ============================================================================
// VecHashMap<Attrib::Key, Attrib::Collection, Attrib::Class::TablePolicy, true, 96u>
// ============================================================================

namespace Attrib
{

// Attrib::Class::TablePolicy::GrowRequest -- the next-larger bucket count
// ((20 * n) / 16 + 3) & ~3, or 1 when n == 0 (X360 slwi/add/slwi/srawi4/addze/+3/clrrwi2).
u32 CollectionHashMap::GrowRequest(u32 luCurrentEntries) const
{
    if (luCurrentEntries == 0)
        return 1u;
    return ((20u * luCurrentEntries) / 16u + 3u) & 0xFFFFFFFCu;
}

// @ 0x828088D0 -- public add: insert, then grow-and-rebuild if the worst probe run
// now exceeds the 96-collision cap.
bool CollectionHashMap::Add(u64 luKey, Collection* lpPtr)
{
    const bool lbAdded = InternalAdd(luKey, lpPtr);

    if (mWorstCollision > KU_WORST_COLLISION_LIMIT)
    {
        CGS_ASSERT(!mFixedAlloc, "Attempted to grow table with fixed allocation");
        RebuildTable(GrowRequest(mTableSize));
    }

    return lbAdded;
}

// @ 0x82806210 -- FindIndex(key): open-addressing probe from ((u32)key % tableSize),
// walking forward up to the home node's recorded max-search count. Returns the matching
// slot index, or mTableSize ("not found") when the key is absent.
u32 CollectionHashMap::FindIndex(u64 luKey) const
{
    if (mNumEntries == 0)
        return mTableSize;

    u32 luIndex = static_cast<u32>(luKey) % mTableSize;
    const u32 luMax = mTable[luIndex].mMax;

    if (luMax != 0)
    {
        u32 luCollisions = 0;
        do
        {
            Node& lrNode = mTable[luIndex];
            if (lrNode.Key() == luKey)
                break;
            CGS_ASSERT(lrNode.IsValid(), "Invalid node found during search; table invariant is broken");
            ++luCollisions;
            luIndex = (luIndex + 1) % mTableSize;
        }
        while (luCollisions < luMax);
    }

    u32 luResult = luIndex;
    if (mTable[luIndex].Key() != luKey)
        luResult = mTableSize;

    return luResult;
}

// @ 0x82806570 -- open-addressing insert with linear probing.
bool CollectionHashMap::InternalAdd(u64 luKey, Collection* lpPtr)
{
    // Full table -> grow first. mNumEntries == mTableSize means every bucket is live.
    if (mNumEntries == mTableSize)
    {
        CGS_ASSERT(!mFixedAlloc, "Attempted to add to table with fixed allocation");
        RebuildTable(GrowRequest(mTableSize));
    }

    const u32 luSize = mTableSize;
    Node* lpBase = mTable;
    u32 luSearchLen = 0;

    // X360: divwu on the LOW 32 bits of the key (clrlwi r11,key,0). twllei r10,0 is the
    // compiler divide-by-zero trap on a zero modulus.
    u32 luIndex = static_cast<u32>(luKey) % luSize;
    // The HOME bucket offset (16 * initial index): fixed for the whole call, it is the
    // slot whose mMax the probe distance is recorded against (X360 r5, never reloaded).
    const u32 luHomeByteOff = 16u * luIndex;
    u32 luByteOff = luHomeByteOff;

    Node* lpProbe = reinterpret_cast<Node*>(reinterpret_cast<u8*>(lpBase) + luByteOff);
    if (!lpProbe->IsEmpty())
    {
        // Probe forward until we hit the key (duplicate -> reject) or an empty slot.
        while (true)
        {
            lpProbe = reinterpret_cast<Node*>(reinterpret_cast<u8*>(lpBase) + luByteOff);

            u64 luCurrent = lpProbe->IsEmpty() ? 0u : lpProbe->mKey;
            if (luCurrent == luKey)
                return false;

            lpBase = mTable;
            ++luSearchLen;
            luIndex = (luIndex + 1u) % luSize;
            luByteOff = 16u * luIndex;

            Node* lpNext = reinterpret_cast<Node*>(reinterpret_cast<u8*>(lpBase) + luByteOff);
            if (lpNext->IsEmpty())
                break;
        }
    }

    // Insert at the found empty slot (lpBase + 16 * luIndex).
    Node* lpSlot = reinterpret_cast<Node*>(reinterpret_cast<u8*>(lpBase) + 16u * luIndex);
    if (lpSlot != NULL)
    {
        lpSlot->mKey = luKey;
        lpSlot->mPtr = lpPtr;
    }

    // Record the probe distance on the HOME bucket (16 * ((u32)luKey % luSize)) and
    // refresh the table-wide worst collision.
    Node* lpHome = reinterpret_cast<Node*>(reinterpret_cast<u8*>(mTable) + luHomeByteOff);
    if (lpHome->mMax < luSearchLen)
        lpHome->mMax = luSearchLen;

    if (luSearchLen > mWorstCollision)
        mWorstCollision = static_cast<u16>(luSearchLen);

    ++mNumEntries;
    return true;
}

// @ 0x82806A28 -- UpdateSearchLength(freedSlot, homeSlot): table-invariant maintenance
// after a remove vacates a slot. Re-find the probe chain rooted at homeSlot, relocate its
// last live member into the freed slot, recompute that chain's max-search count, and --
// when the map was at its worst-collision bound -- rescan the whole table to recompute the
// high-water mark. Returns the relocated node's new index, or (u32)-1 when the chain could
// not be rebalanced.
u32 CollectionHashMap::UpdateSearchLength(u32 luFreedSlot, u32 luHomeSlot)
{
    u32 luProbe = luFreedSlot;

    if (luFreedSlot == luHomeSlot)
    {
        if (mTable[luHomeSlot].mMax == 0)
        {
            const u32 luTableSize = mTableSize;
            const u32 luWorst     = mWorstCollision;
            luProbe = (luTableSize - luWorst + luHomeSlot) % luTableSize;
            if (mTable[luProbe].mMax < luWorst)
            {
                u32 luRemaining = luWorst;
                while (luRemaining != 0)
                {
                    --luRemaining;
                    luProbe = (luProbe + 1) % luTableSize;
                    if (mTable[luProbe].mMax >= luRemaining)
                        break;
                }
                if (luRemaining == 0)
                    return static_cast<u32>(-1);
            }
            else if (luWorst == 0)
            {
                return static_cast<u32>(-1);
            }
        }
        else
        {
            luProbe = luHomeSlot;
        }
    }

    const u32 luMax = mTable[luProbe].mMax;
    CGS_ASSERT(luMax <= mWorstCollision, "Error in table invariant.");

    const u32 luTableSize = mTableSize;
    const u32 luLast = (luMax + luProbe) % luTableSize;
    Node& lrLast = mTable[luLast];
    if (lrLast.IsValid())
    {
        CGS_ASSERT(luProbe == (lrLast.Key() % luTableSize), "Incorrect max search length found in table.");
    }

    CGS_ASSERT(!mTable[luFreedSlot].IsValid(), "Free node is not invalid!");

    if (luFreedSlot != luLast)
    {
        Node& lrFreed = mTable[luFreedSlot];
        lrFreed.mKey = lrLast.mKey;
        lrFreed.mPtr = lrLast.mPtr;
        lrLast.mKey  = 0;
        lrLast.mPtr  = reinterpret_cast<Attrib::Collection*>(&lrLast);
    }

    CGS_ASSERT(!mTable[luLast].IsValid(), "Freed node is not invalid!");

    u32 luNewMax = 0;
    for (u32 luStep = 1; luStep < luMax; ++luStep)
    {
        Node& lrNode = mTable[(luStep + luProbe) % luTableSize];
        const u64 luKey = lrNode.Key();
        if (luKey % luTableSize == luProbe)
            luNewMax = luStep;
    }

    CGS_ASSERT(luNewMax <= mWorstCollision, "Worst search got worse after making an improvement!");

    mTable[luProbe].mMax = luNewMax;

    const u32 luWorst = mWorstCollision;
    if (luMax == luWorst && mTable[luFreedSlot].mMax < luWorst && luNewMax < luMax)
    {
        const u32 luEntries = mTableSize;
        mWorstCollision = 0;
        if (luEntries != 0)
        {
            u32 luScan = 0;
            do
            {
                const u32 luCurWorst = mWorstCollision;
                if (luCurWorst >= luWorst)
                    break;
                const u32 luNodeMax = mTable[luScan].mMax;
                if (luNodeMax > luCurWorst)
                    mWorstCollision = static_cast<u16>(luNodeMax);
                ++luScan;
            }
            while (luScan < mTableSize);
        }
    }

    return luLast;
}

// @ 0x82806828 -- (re)allocate the bucket array and re-home every live entry, retrying
// with one more bucket each pass until the worst collision drops to 96 or below.
void CollectionHashMap::RebuildTable(u32 luNewSize)
{
    if (luNewSize == 0)
    {
        // Release path: only free when the table is neither fixed nor holding entries.
        if (mFixedAlloc == 0 && mNumEntries == 0 && mTable != NULL)
        {
            TableFreeFunc(mTable, static_cast<size_t>(mTableSize) << 4);
            mTable = NULL;
        }
        return;
    }

    u16 luSize = static_cast<u16>(luNewSize);
    do
    {
        const u16   luOldSize  = mTableSize;
        Node* const lpOldTable = mTable;

        mNumEntries     = 0;
        mWorstCollision = 0;
        mTableSize      = luSize;

        // TablePolicy::Alloc: ((16 * luSize) masked to 16 bits << 4) & 0xFFFF0 bytes from
        // the AttribSys package allocator (X360 reads &sAttribSysAllocator directly; the
        // sbHasLinearAllocator assert is the GetAttribSysAllocator() guard).
        CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::HasMemoryBuffer(), "sbHasLinearAllocator");
        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        const size_t lnBytes = (static_cast<size_t>(luSize) << 4) & 0xFFFF0u;
        mTable = static_cast<Node*>(lpAllocator->Malloc(lnBytes, 0));

        CopyFromOldTable(lpOldTable, luOldSize, true);

        ++luSize;
    }
    while (mWorstCollision > KU_WORST_COLLISION_LIMIT);
}

// @ 0x828066F0 -- clear this (freshly-allocated) table to all-empty, then re-insert
// every live entry of the old table; optionally free the old bucket array.
void CollectionHashMap::CopyFromOldTable(Node* lpOldTable, u32 luOldSize, bool lbFreeOld)
{
    // Empty every bucket of the (new) table: key 0, self-pointer sentinel, mMax 0.
    if (mTableSize != 0)
    {
        u32 luByteOff = 0;
        for (u32 luIndex = 0; luIndex < mTableSize; ++luIndex)
        {
            Node* lpNode = reinterpret_cast<Node*>(reinterpret_cast<u8*>(mTable) + luByteOff);
            if (lpNode != NULL)
            {
                lpNode->mKey = 0;
                lpNode->mPtr = reinterpret_cast<Collection*>(lpNode); // self == empty
                lpNode->mMax = 0;
            }
            luByteOff += 16u;
        }
    }

    if (lpOldTable != NULL)
    {
        // Re-home every live entry from the old table.
        if (luOldSize != 0)
        {
            Node* lpOld = lpOldTable;
            u32   luRemaining = luOldSize;
            do
            {
                Collection* lpEntry = lpOld->mPtr;
                if (reinterpret_cast<const void*>(lpEntry) != reinterpret_cast<const void*>(lpOld))
                    InternalAdd(lpOld->mKey, lpEntry);
                --luRemaining;
                lpOld = reinterpret_cast<Node*>(reinterpret_cast<u8*>(lpOld) + 16u);
            }
            while (luRemaining != 0);
        }

        if (lbFreeOld)
        {
            // Inlined TablePolicy::Free: hand the old array back to the AttribSys package
            // allocator's heap (X360 reads sAttribSysAllocator.mpHeapAllocator directly and
            // calls CgsMemory::HeapMalloc::Free(heap, oldTable); the sbHasLinearAllocator /
            // mbHasAllocator asserts are the same GetAttribSysAllocator() guards), then bump
            // the freed-byte census.
            CGS_ASSERT(CgsAttribSys::AttribSysMemoryManager::HasMemoryBuffer(), "sbHasLinearAllocator");
            CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
                CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
            lpAllocator->Free(lpOldTable, static_cast<size_t>(luOldSize) << 4);
            gTablePolicyFreedBytes += 16u * luOldSize;
        }
    }
}

// @ 0x8280DBA8 -- destroy every live collection, release the bucket array unless
// fixed, then zero the counts.
void CollectionHashMap::Clear()
{
    if (mTableSize != 0)
    {
        u32 luByteOff = 0;
        for (u32 luIndex = 0; luIndex < mTableSize; ++luIndex)
        {
            if (mNumEntries == 0)
                break;

            Node* lpNode = reinterpret_cast<Node*>(reinterpret_cast<u8*>(mTable) + luByteOff);
            Collection* lpEntry = lpNode->mPtr;
            if (reinterpret_cast<const void*>(lpEntry) != reinterpret_cast<const void*>(lpNode))
            {
                if (lpEntry != NULL)
                    Collection_ScalarDeletingDtor(lpEntry, 1);
                --mNumEntries;
            }

            luByteOff += 16u;
        }
    }

    if (mFixedAlloc == 0)
    {
        if (mTable != NULL)
            TableFreeFunc(mTable, static_cast<size_t>(mTableSize) << 4);
        mTable     = NULL;
        mTableSize = 0;
    }

    mNumEntries     = 0;
    mWorstCollision = 0;
}

} // namespace Attrib
