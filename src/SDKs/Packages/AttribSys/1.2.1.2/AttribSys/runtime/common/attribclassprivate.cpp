#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // HashMapTablePolicy::Free
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h"           // AttribListNode/Base + delete-queue seam
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h"    // Attrib::DatabasePrivate (named registry members)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h"          // Attrib::Vault (AddRef / live-class count)

// AttribSys runtime -- Attrib::ClassPrivate bodies, reconstructed from
// BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//   Attrib::ClassPrivate::ClassPrivate  @ 0x8280EC80
//   Attrib::ClassPrivate::Release       @ 0x8280C370
//   Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
//
// 2026-07-27 (attrib-sdk wave): re-homed onto the named-member x64 layout
// (attribclassprivate.h). The earlier raw-byte-offset bodies wrote X360 offsets
// with widened pointer stores, which overlapped on the 64-bit host (mpDefinitions
// @+0x2C vs mpSource @+0x30) -- retired per the x64 semantic-parity rule. The
// layout HashMap / collection table / class registry are now driven through the
// committed real container types (attribhashmap.cpp / vechashmap.cpp).

namespace Attrib
{
    // Attrib::DatabasePrivate::QueueForDelete<Attrib::Class> @ 0x8280BF78. The Class twin of
    // the Collection delete-queue push (attribcollection.cpp). Identical ring logic; the
    // referenced-check reads the shared layout-table refcount through the Class's
    // ClassPrivate. Reached from Attrib::ClassPrivate::Release on the final drop.
    void* DatabasePrivate_QueueClassForDelete(void* lpClass, void* lpGarbageList)
    {
        AttribListBase* lpQueue = reinterpret_cast<AttribListBase*>(lpGarbageList);

        // X360: refcount = ClassPrivate(mpPrivates)->mLayoutTable.muRefCount.
        bool lbNullOrReferenced = (lpClass == NULL);
        if (!lbNullOrReferenced)
        {
            const ClassPrivate* lpClassPrivate =
                reinterpret_cast<const ClassPrivate*>(lpClass)->mpPrivates;
            lbNullOrReferenced = (lpClassPrivate->mLayoutTable.GetRefCount() != 0);
        }
        CGS_ASSERT(!lbNullOrReferenced,
                   "Cannot queue NULL or referenced object for delete.");

        // Already queued? (walk the ring; the X360 leaves r3 = the object, unused by the caller.)
        for (AttribListNode* lpNode = lpQueue->mNode.mpNext;
             lpNode != &lpQueue->mNode; lpNode = lpNode->mpNext)
        {
            if (lpNode->mpValue == lpClass)
                return lpClass;
        }

        // Allocate a node holding the class and link it at the tail (insert-before-sentinel).
        AttribListNode* lpNewNode = AttribListAllocateNode(lpQueue, &lpClass);
        lpNewNode->mpNext              = &lpQueue->mNode;
        lpNewNode->mpPrev              = lpQueue->mNode.mpPrev;
        lpQueue->mNode.mpPrev->mpNext  = lpNewNode;
        lpQueue->mNode.mpPrev          = lpNewNode;
        return lpNewNode;
    }

    // The class-registry EraseAt (X360 sub_82808A98 -- the VecHashMap<false,16>
    // slot-vacate with probe-run repair). Teardown-path only (~ClassPrivate);
    // deferred to its own TU with the remaining VecHashMap removal family.
    static void ClassTable_EraseAt(ClassTable& lrTable, unsigned int luIndex)
    {
        (void)lrTable; (void)luIndex;
        CGS_ASSERT(false, "ClassTable EraseAt @0x82808A98: deferred TU (teardown path)");
    }
}

// ============================================================================
// Attrib::ClassPrivate::ClassPrivate @ 0x8280EC80
// ============================================================================
// Build the private impl of an attribute Class from its serialised load record:
// copy the Class base key, install the self back-pointer, construct the layout
// HashMap from the load's {count, key-shift} (dynamic), zero + optionally
// reserve the collection table, seed the definition/layout metadata, take a
// layout-table reference and the source Vault's live-class reference, push every
// laid-out searchable definition into the layout HashMap, and finally register
// the Class with the process database's class table.
Attrib::ClassPrivate::ClassPrivate(const ClassLoadData& lrLoad, Vault* lpSource)
    : mKey(lrLoad.mClass)
    , mpPrivates(this)
    , mLayoutTable(lrLoad.mLayoutCount, static_cast<u8>(lrLoad.mLayoutKeyShift), 1)
{
    // mCollections: zero the table header in place, then reserve when the load
    // record carries a seed (X360 TablePolicy_1_96_::Rebuild gated on it).
    mCollections = CollectionHashMap();
    if (lrLoad.mCollectionReserve != 0)
        mCollections.RebuildTable(lrLoad.mCollectionReserve);

    mLayoutSize = static_cast<u16>(lrLoad.mLayoutSize);
    mNumDefinitions = static_cast<u16>(lrLoad.mNumDefinitions);
    mDefinitions = lrLoad.GetDefinitions();
    mSource = lpSource;

    // Layout-table shared refcount (overflow guard + AddRef), then the source
    // Vault's live-class count (X360 ++vault->mRefCount @+16).
    mLayoutTable.AddRef();
    mSource->AddRef(0);   // the source Vault gains one live-class reference

    // Push every laid-out (flag bit1), searchable (not bit3) definition into the
    // layout HashMap. The X360 re-reads the self back-pointer each iteration.
    ClassPrivate* lpSelf = mpPrivates;
    if (lpSelf->mNumDefinitions != 0)
    {
        for (unsigned int luIndex = 0; luIndex < lpSelf->mNumDefinitions; ++luIndex)
        {
            const Definition& lrDef = lpSelf->mDefinitions[luIndex];
            if ((lrDef.mFlags & 2) != 0 && (lrDef.mFlags & 8) == 0)
            {
                // X360 stages Add(key, typeKey, {offset,size} word, 0, addFlags,
                // 1, 0): the layout node's payload is the def's packed
                // {mOffset << 16 | mSize} word (the X360's 32-bit load at
                // def+16), the type rides as the 64-bit key (the Node ctor
                // resolves the index), the laid-out rebase is OFF (r7 = 0), and
                // the node flags are (flags&1)<<1 | (laid-out ? 0x10 : 0x20).
                const u32 luPacked =
                    (static_cast<u32>(lrDef.mOffset) << 16) | lrDef.mSize;
                const u8 lu8AddFlags = static_cast<u8>(
                    ((lrDef.mFlags & 1) << 1) |
                    (((lrDef.mFlags & 2) != 0) ? 0x10 : 0x20));
                lpSelf->mLayoutTable.Add(lrDef.mKey,
                                         lrDef.mType,
                                         reinterpret_cast<void*>(
                                             static_cast<uintptr_t>(luPacked)),
                                         false,
                                         lu8AddFlags,
                                         true,
                                         NULL);
            }
        }
    }

    // Register this Class with the process attribute database's class table
    // (the X360 reads off_83011BC4->mPrivates, asserting initialization first).
    DatabasePrivate* lpPrivates = GetDatabasePrivate();
    lpPrivates->mClasses.Add(mKey, reinterpret_cast<Attrib::Class*>(this));
}

// ============================================================================
// Attrib::ClassPrivate::Release @ 0x8280C370
// ============================================================================
// Drop one reference on the layout HashMap; once it hits zero, queue this Class for
// deferred deletion on the database's garbage list. Returns the HashMap release result
// (a byte-wide bool widened to int, exactly as the X360 threads r3 through).
int Attrib::ClassPrivate::Release()
{
    const int liReleased = static_cast<int>(mLayoutTable.Release());
    if (liReleased != 0)
    {
        DatabasePrivate* lpPrivates = GetDatabasePrivate();
        return static_cast<int>(reinterpret_cast<intptr_t>(
            DatabasePrivate_QueueClassForDelete(this, &lpPrivates->mGarbageClasses)));
    }
    return liReleased;
}

// ============================================================================
// Attrib::ClassPrivate::~ClassPrivate @ 0x8280F4D8
// ============================================================================
// Tear down the private Class impl: clear the layout HashMap's count, drop the source
// Vault's live-class refcount (deleting the Vault when it reaches zero), unregister the
// Class from the database class table, clear the collection table, then release the
// layout HashMap's bucket buffer.
Attrib::ClassPrivate::~ClassPrivate()
{
    mLayoutTable.ClearCountForTeardown();

    // Drop the source Vault's live-class refcount; delete it when it hits zero.
    if (mSource->Release(0))
        Vault_ScalarDeletingDtor(mSource, 1);

    // Unregister the Class from the database class table.
    DatabasePrivate* lpPrivates = GetDatabasePrivate();
    const unsigned int luIndex = lpPrivates->mClasses.FindIndex(mKey);
    ClassTable_EraseAt(lpPrivates->mClasses, luIndex);

    mCollections.Clear();

    CGS_ASSERT(mLayoutTable.GetCount() == 0,
               "Attrib::HashMap not empty when destroyed.");

    mLayoutTable.ReleaseBucketsForTeardown();
}
