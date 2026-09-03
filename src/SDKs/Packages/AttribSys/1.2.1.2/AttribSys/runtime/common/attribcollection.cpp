#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"      // Attrib::Node
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"    // Array/TypeDesc/ITypeHandler
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h" // AttribListNode/Base + delete-queue seam
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h" // Attrib::DatabasePrivate (named registry members)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribclassprivate.h"    // Attrib::ClassPrivate (named layout table / static data)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"   // Attrib::Free
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsys.h"             // Attrib::Database (load ctor type lookup)
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribloadandgo.h" // Attrib::Vault (the collection source AddRef)
#include <new>                                                                        // placement new (the load ctor site)

#include <cstdint> // intptr_t, uintptr_t
#include "GameShared/GameClasses/Core/CgsAssert.h"

// AttribSys runtime -- Attrib::Collection refcount + attribute-table bodies, reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//   Attrib::Collection::AddRef       @ 0x828028E0
//   Attrib::Collection::Release      @ 0x8280C2E8
//   Attrib::Collection::GetNode      @ 0x82804EF0
//   Attrib::Collection::GetData      @ 0x82804FD0
//   Attrib::Collection::FreeNodeData @ 0x8280A068
//   Attrib::Collection::Clear        @ 0x8280AE60
//
// A Collection IS-A Attrib::HashMap (attribhashmap.h) and drives the shared table directly.
// The class-owned internals it reaches through (the Class object's private impl and its
// embedded inherited-layout HashMap / inherited data area) are AttribSys-owned and reached
// by the recovered byte offsets -- the same convention the committed Array::GetTypeDesc and
// Attribute::GetInternalPointer bodies use. Attrib::ScanForValidKey<Attrib::HashMap>
// (@0x82803CC0, mangled ??$ScanForValidKey@VHashMap@Attrib@@@Attrib@@YA_KABVHashMap@0@I@Z)
// and Collection::NextKey (@0x828050D0) are homed here as of the collection-load wave --
// the load ctor's default-layout pass is their first caller.
//
// A Collection IS-A Attrib::HashMap (attribhashmap.h): the X360 passes a Collection*
// straight into HashMap::Release and reaches the shared refcount at the HashMap +0x08
// slot. AddRef therefore bumps the inherited muRefCount, and Release forwards to the real
// HashMap::Release and -- on the final drop -- queues the collection onto the attribute
// database's garbage list. The scalar deleting destructor (@0x8280C510) is homed beside
// its CollectionHashMap::Clear caller in vechashmap.cpp.

namespace Attrib
{
    // Read the process attribute database's private impl (off_83011BC4 followed
    // to mPrivates; asserts the database is initialized). Typed against the real
    // x64 DatabasePrivate (attribdatabaseprivate.h); bodied in attribdatabase.cpp.
    struct DatabasePrivate;
    DatabasePrivate* GetDatabasePrivate();

    // Attrib::DatabasePrivate::QueueForDelete<Attrib::Collection> @ 0x8280BEA8. Defer a
    // collection for garbage collection on the database's collection garbage list (an intrusive
    // eastl::list ring, AttribListBase). The X360 asserts the object is non-NULL and unreferenced
    // (its shared HashMap refcount at +0x08 is zero), then -- only if it is not already queued --
    // allocates a node holding the collection and links it at the tail of the ring. Reached from
    // Attrib::Collection::Release on the final refcount drop.
    void* DatabasePrivate_QueueCollectionForDelete(void* lpCollection, void* lpGarbageList)
    {
        AttribListBase* lpQueue = reinterpret_cast<AttribListBase*>(lpGarbageList);

        // The X360 reads the collection's shared refcount at +0x08 (the HashMap base's
        // muRefCount). FIXED (attrib teardown wave 2026-09-03): this used to transcribe
        // that CONSOLE offset literally -- on the host mpBuckets is 8 bytes, so +0x08 is
        // HashMap::muCapacity, and the "referenced" guard was reading the BUCKET COUNT of
        // the table being queued. Read it BY NAME (the identical defect attribhashmap.cpp's
        // HashMap::Release note documents).
        bool lbNullOrReferenced = (lpCollection == NULL);
        if (!lbNullOrReferenced)
        {
            const u16 lu16RefCount =
                static_cast<const Collection*>(lpCollection)->GetRefCount();
            lbNullOrReferenced = (lu16RefCount != 0);
        }
        CGS_ASSERT(!lbNullOrReferenced,
                   "Cannot queue NULL or referenced object for delete.");

        // Already queued? (walk the ring; the X360 leaves r3 = the object, unused by the caller.)
        for (AttribListNode* lpNode = lpQueue->mNode.mpNext;
             lpNode != &lpQueue->mNode; lpNode = lpNode->mpNext)
        {
            if (lpNode->mpValue == lpCollection)
                return lpCollection;
        }

        // Allocate a node holding the collection and link it at the tail (insert-before-sentinel).
        AttribListNode* lpNewNode = AttribListAllocateNode(lpQueue, &lpCollection);
        lpNewNode->mpNext              = &lpQueue->mNode;
        lpNewNode->mpPrev              = lpQueue->mNode.mpPrev;
        lpQueue->mNode.mpPrev->mpNext  = lpNewNode;
        lpQueue->mNode.mpPrev          = lpNewNode;
        return lpNewNode;
    }
}

// ============================================================================
// Attrib::Collection::~Collection @ 0x8280C3F8
// ============================================================================
// The full teardown, store-for-store from 0x8280C3F8..0x8280C50C. In X360 order:
//   0x8280C40C  assert the shared refcount (HashMap base +0x08) is zero
//   0x8280C438  Class::RemoveCollection(mpClass, this)  -- unregister from the
//               owning class's collection table
//   0x8280C444  ClassPrivate::Release(mpClass->mpPrivates) -- drop the reference the
//               load ctor took on the class's shared layout table
//   0x8280C450  if (mpParent) mpParent->Release()       -- the parent/default chain
//   0x8280C464  Clear()                                  -- free every attribute
//   0x8280C46C  if (mpSource)  --vault->mRefCount, delete the vault on the final drop
//               else if (mpData) Attrib::Free(mpData, classPrivate->mLayoutSize,
//                                             "Attrib::layout")
//   0x8280C4B8  ~HashMap, INLINED: assert the table is empty (attribhashmap.h:412)
//               then release the bucket array
// The object memory itself is NOT freed here: both callers own that -- the GC bag drain
// (DatabasePrivate::CollectGarbageBag<Collection> @0x8280E3C0) and the scalar deleting
// destructor (@0x8280C510) each run this body and then hand the collection block back
// to the AttribSys package allocator themselves.
//
// Every field is reached BY NAME. The X360 byte offsets quoted above are the console's
// (mpBuckets is 4 bytes there, 8 here), so none of them may be carried onto the host.
// ============================================================================
Attrib::Collection::~Collection()
{
    // 0x8280C40C `lhz r11,8(r31)` -- the HashMap base's shared refcount.
    CGS_ASSERT(muRefCount == 0,
               "Destroying Attrib::Collection that is still referenced.");

    // 0x8280C438 `mr r4,r31 ; lwz r3,0x18(r31) ; bl Attrib::Class::RemoveCollection`
    // -- r3 = mpClass, r4 = this.
    // [GUARD] mpClass == NULL: a collection built from an empty key (Attrib::Gen::* temporaries with key 0,
    // e.g. AICar::SetDriver on a car whose asset key is unset) has no class; the console never garbage-
    // collects such a collection, this build did (run2: AV in Class::RemoveCollection reading class+8).
    // [GUARD] a CLASS-LESS collection (mpClass == NULL: an Attrib::Gen::* temporary built from key 0,
    // e.g. AICar::SetDriver on a car whose asset key is unset) owns nothing the console's body would
    // hand back -- no class registration, no layout, no attributes (Clear @0x8280C464 reads
    // mpClass->mpPrivates) -- and the console never garbage-collects one (run2/run3: AVs in
    // Class::RemoveCollection and Collection::Clear reading class+8). Only its own buckets go.
    if (mpClass == NULL)
    {
        ReleaseBucketsForTeardown();
        return;
    }

    ClassPrivate* lpClassPrivate = NULL;
    if (mpClass != NULL)
    {
        mpClass->RemoveCollection(this);
    
        // 0x8280C444 `lwz r11,0x18(r31) ; lwz r3,8(r11) ; bl Attrib::ClassPrivate::Release`
        // -- class+8 is the ClassPrivate; Release drops the layout table's shared refcount
        // and queues the class for deferred deletion on the final drop.
        lpClassPrivate = reinterpret_cast<ClassPrivate*>(mpClass)->mpPrivates;
        lpClassPrivate->Release();
    }

    // 0x8280C450 `lwz r3,0xC(r31) ; cmplwi ; beq ; ld r4,0x10(r31) ; bl
    // Attrib::Collection::Release`. The `ld r4,0x10(r31)` stages mKey into r4, which
    // Collection::Release @0x8280C2E8 never reads (its whole body only touches r3 and
    // the database singleton) -- a dead scheduled load, NOT a second argument.
    if (mpParent != NULL)
        mpParent->Release();

    // 0x8280C464 -- free every attribute this collection owns, plus this instance's
    // data for every laid-out/inherited attribute of the class layout table.
    Clear();

    if (mpSource != NULL)
    {
        // 0x8280C478 `lwz r11,0x10(r3) ; addic. r11,r11,-1 ; stw r11,0x10(r3) ; bne ;
        // li r4,1 ; bl Attrib::Vault::`scalar deleting destructor`' -- the vault
        // reference the load ctor took (Vault::mRefCount @ X360 +16), destroyed on the
        // final drop. Same shape as ~ClassPrivate's source-vault release.
        if (mpSource->Release(0))
            Vault_ScalarDeletingDtor(mpSource, 1);
    }
    else if (mpData != NULL && lpClassPrivate != NULL)   // ([GUARD] no class -> no layout size to hand back)
    {
        // 0x8280C494 -- no source vault, so the layout block belongs to this collection:
        // hand it back sized by the owning class's layout size (`lwz r10,0x18(r31) ;
        // lwz r11,8(r10) ; lhz r4,0x28(r11)` == ClassPrivate::mLayoutSize).
        Attrib::Free(mpData, lpClassPrivate->mLayoutSize, "Attrib::layout");
    }

    // ---- ~HashMap, inlined by the X360 at 0x8280C4B8..0x8280C4F8 ------------------
    // `lhz r11,6(r31)` == muCount, then `lwz r3,0(r31) ; lhz r11,4(r31) ;
    // rotlwi r4,r11,4 ; bl Attrib::HashMapTablePolicy::Free` == Free(mpBuckets,
    // capacity * 16). The console's 16 is sizeof(Node) THERE; ReleaseBucketsForTeardown
    // (attribhashmap.cpp -- the same seam ~ClassPrivate uses) is the sizeof-based host
    // equivalent.
    CGS_ASSERT(GetCount() == 0, "Attrib::HashMap not empty when destroyed.");
    ReleaseBucketsForTeardown();
}

// ============================================================================
// Attrib::Collection::AddRef @ 0x828028E0
// ============================================================================
// Bump the shared refcount (the HashMap base's muRefCount @ +0x08), asserting it has not
// already saturated at 0xFFFF, then return this. This is the out-of-line copy of the
// ++refcount-with-overflow-guard the X360 inlines at the other AddRef sites.
Attrib::Collection* Attrib::Collection::AddRef()
{
    CGS_ASSERT(muRefCount != 0xFFFF, "Exceeded collection refcount maximum!\n");
    ++muRefCount;
    return this;
}

// ============================================================================
// Attrib::Collection::Release @ 0x8280C2E8
// ============================================================================
// Drop one reference on the collection's attribute table (HashMap::Release). Once it hits
// zero, queue this collection for deferred deletion on the attribute database's collection
// garbage list (mPrivates + 0x6C). Returns the HashMap release result (a byte-wide bool
// widened to int, exactly as the X360 threads r3 through), or the QueueForDelete result on
// the final drop.
int Attrib::Collection::Release()
{
    const int liReleased = HashMap::Release() ? 1 : 0;
    if (liReleased != 0)
    {
        // The database's deferred-collection ring (X360 mPrivates+0x6C), by name
        // now the x64 DatabasePrivate layout is real.
        return static_cast<int>(reinterpret_cast<intptr_t>(
            DatabasePrivate_QueueCollectionForDelete(
                this, &GetDatabasePrivate()->mGarbageCollections)));
    }
    return liReleased;
}

// ============================================================================
// Attrib::Collection::GetNode @ 0x82804EF0
// ============================================================================
// Resolve the schema Node for an attribute key. Walk this collection and its parent/default
// chain (mpParent), returning the first collection whose open-addressed table holds the key
// -- reporting that owning collection through lrpContainer. When the whole chain misses,
// fall back (only if this collection carries instance data) to the owning class's shared
// inherited-layout table (the HashMap embedded at classPrivate + 0x10). Returns null with a
// null container when the key is absent everywhere.
Attrib::Node* Attrib::Collection::GetNode(u64 luKey, const Collection*& lrpContainer) const
{
    for (const Collection* lpCollection = this; lpCollection != NULL;
         lpCollection = lpCollection->mpParent)
    {
        Attrib::Node* lpNode = reinterpret_cast<Attrib::Node*>(lpCollection->Find(luKey));
        if (lpNode != NULL)
        {
            lrpContainer = lpCollection;
            return lpNode;
        }
    }

    if (mpData != NULL)
    {
        // The owning class's shared layout table (X360 classPrivate+0x10),
        // by name now the x64 ClassPrivate layout is real.
        const ClassPrivate* lpClassPrivate =
            reinterpret_cast<const ClassPrivate*>(mpClass)->mpPrivates;
        const HashMap* lpLayoutTable = &lpClassPrivate->mLayoutTable;

        Attrib::Node* lpNode = reinterpret_cast<Attrib::Node*>(lpLayoutTable->Find(luKey));
        if (lpNode != NULL)
        {
            lrpContainer = this;
            return lpNode;
        }
    }

    lrpContainer = NULL;
    return NULL;
}

// ============================================================================
// Attrib::Collection::GetData @ 0x82804FD0
// ============================================================================
// Raw data pointer for element luIndex of the attribute keyed by luKey. Resolve the node
// (and its owning collection) via GetNode. For an array-typed node (flag 0x2) locate the
// Array header -- at the owner's data area + node offset when laid out (0x10), in the owning
// class's inherited data area when inherited (0x20), or the node's own value word when plain
// -- then index it. For a non-array node only element 0 is valid; return the node's
// instance-resolved pointer against the owner's data area.
void* Attrib::Collection::GetData(u64 luKey, unsigned int luIndex) const
{
    const Collection* lpOwner = NULL;
    Attrib::Node* lpNode = GetNode(luKey, lpOwner);
    if (lpNode == NULL)
        return NULL;

    const u8 lu8Flags = lpNode->muFlags;
    if ((lu8Flags & 0x2u) != 0)
    {
        Array* lpArray;
        if ((lu8Flags & 0x10u) != 0)
        {
            // Laid out: Array header at the owner's data area + node offset.
            lpArray = reinterpret_cast<Array*>(
                reinterpret_cast<u8*>(lpOwner->mpData) + lpNode->muValue);
        }
        else if ((lu8Flags & 0x20u) != 0)
        {
            // Inherited: Array header in the owning class's inherited data area
            // (classPrivate + 0x34), reached by the recovered offsets.
            // The class's inherited data area (X360 classPrivate+0x34 ==
            // mStaticData), by name.
            const ClassPrivate* lpOwnerPrivate =
                reinterpret_cast<const ClassPrivate*>(lpOwner->mpClass)->mpPrivates;
            u8* lpDataBase = static_cast<u8*>(lpOwnerPrivate->mStaticData);
            lpArray = reinterpret_cast<Array*>(lpDataBase + lpNode->muValue);
        }
        else
        {
            // Plain: the node's value word is the (X360 32-bit) Array pointer image.
            lpArray = reinterpret_cast<Array*>(lpNode->mpValue);   // full-width payload
        }
        return lpArray->GetData(luIndex);
    }

    CGS_ASSERT(luIndex == 0, "Cannot get non-array data from a non-zero index.");
    // X360 `lwz r5,var_20(r1); lwz r4,0x1C(r5); bl Node::GetPointer` -- the container
    // collection rides in r5 as GetPointer's third argument (its 0x20/inherited branch
    // dereferences it). The one-argument spelling this used to call was a fork; retired
    // 2026-07-31 when the real GetPointer @0x828045B0 landed.
    return lpNode->GetPointer(lpOwner->mpData, lpOwner);
}

// ============================================================================
// Attrib::Collection::FreeNodeData @ 0x8280A068
// ============================================================================
// Release one attribute's backing data. For an array (lbIsArray) release every element
// through the schema type handler -- the element pointer is Array::GetData(i), the inline
// value pointer for value arrays or the dereferenced stored pointer for reference arrays
// (the exact address the X360 computes inline) -- then, if the array owns its allocation
// (lbRequiresRelease), free the whole Array block. For a single value, release it through
// the handler, then free the block via Attrib::Free (sized by the TypeDesc field the X360
// reads at +0x0C). `this` is unused -- the X360 body never touches it.
void Attrib::Collection::FreeNodeData(bool lbIsArray, void* lpData, bool lbRequiresRelease,
                                      const TypeDesc& lrTypeDesc) const
{
    ITypeHandler* lpHandler = lrTypeDesc.mHandler;

    if (lbIsArray)
    {
        Array* lpArray = reinterpret_cast<Array*>(lpData);
        if (lpHandler != NULL)
        {
            for (u32 luIndex = 0; luIndex < lpArray->muNumElements; ++luIndex)
                lpHandler->Release(lpArray->GetData(luIndex));
        }
        if (lbRequiresRelease)
            Array::Destroy(lpArray);
    }
    else
    {
        if (lpHandler != NULL)
            lpHandler->Release(lpData);
        if (lbRequiresRelease)
            // X360 `lwz r4,0xC(r30)` == TypeDesc+0x0C == mSize -- the value's byte size
            // drives the live-byte census decrement (and Free skips a zero size). This
            // passed mIndex (the +0x10 field) until 2026-08-04: census corruption, and a
            // straight LEAK for any type whose compiled index is 0.
            Free(lpData, lrTypeDesc.mSize, "Attrib::attribute_data");
    }
}

// ============================================================================
// Attrib::Collection::Clear @ 0x8280AE60
// ============================================================================
// Empty the collection completely. Phase 1 removes every attribute this collection owns:
// walk its own bucket array, and for each occupied node resolve its schema TypeDesc, unhook
// it from the table (HashMap::Remove without search-length repair -- the table is being torn
// down), and free its backing data. Phase 2 walks the owning class's shared layout table and
// frees this instance's data for every laid-out/inherited attribute (the shared table itself
// is never modified). Finally assert the number of attributes cleared matches the collection
// census (own live entries + class-layout entries).
void Attrib::Collection::Clear()
{
    unsigned int luCleared = 0;

    // Expected total = this collection's own live entries + the class layout-table entries.
    // The owning class's private impl + shared layout table (X360 class+0x08 /
    // classPrivate+0x10), by name now the x64 ClassPrivate layout is real.
    ClassPrivate* lpClassPrivate =
        reinterpret_cast<ClassPrivate*>(mpClass)->mpPrivates;
    HashMap* lpLayout = &lpClassPrivate->mLayoutTable;
    const unsigned int luExpected =
        static_cast<unsigned int>(lpLayout->muCount) + static_cast<unsigned int>(muCount);

    // ---- Phase 1: remove every attribute this collection owns -----------------------
    unsigned int luCapacity = muCapacity;
    unsigned int luIndex = 0;
    while (luIndex < luCapacity && !mpBuckets[luIndex].IsOccupied())
        ++luIndex;

    while (luIndex < luCapacity && mpBuckets[luIndex].IsOccupied())
    {
        HashMap::Node* lpBucket = &mpBuckets[luIndex];
        CGS_ASSERT(lpBucket->IsOccupied(), "Invalid node found at valid index.");

        const u8 lu8Flags = lpBucket->mFlags;
        const bool lbRequiresRelease = (lu8Flags & 0x1u) != 0;
        const bool lbIsArray         = (lu8Flags & 0x2u) != 0;

        // Resolve the node's schema TypeDesc from the attribute database's indexed-type
        // vector (X360 mPrivates +0x14 count / +0x18/+0x1C begin/end -- the named
        // mNumCompiledTypes/mCompiledTypes now the x64 DatabasePrivate layout is real).
        // The index is clamped to 0 when out of range, then bounds-checked (the
        // inlined EASTL vector::operator[]).
        DatabasePrivate* lpPrivates = GetDatabasePrivate();
        const u32 luNumTypes  = lpPrivates->mNumCompiledTypes;
        u32 luTypeIndex = lpBucket->mTypeIndex;
        if (luTypeIndex >= luNumTypes)
            luTypeIndex = 0;
        const TypeDesc** lppBegin = lpPrivates->mCompiledTypes.mpBegin;
        const TypeDesc** lppEnd   = lpPrivates->mCompiledTypes.mpEnd;
        CGS_ASSERT(luTypeIndex < static_cast<u32>(lppEnd - lppBegin),
                   "!\"vector::operator[] -- out of range\"");
        const TypeDesc* lpTypeDesc = lppBegin[luTypeIndex];

        void* lpPayload = Remove(lpBucket, mpData, this, false);
        ++luCleared;
        if (lpPayload != NULL)
            FreeNodeData(lbIsArray, lpPayload, lbRequiresRelease, *lpTypeDesc);

        luCapacity = muCapacity;
        ++luIndex;
        while (luIndex < luCapacity && !mpBuckets[luIndex].IsOccupied())
            ++luIndex;
    }

    // ---- Phase 2: free this instance's data for every laid-out/inherited attribute ---
    lpClassPrivate = reinterpret_cast<ClassPrivate*>(mpClass)->mpPrivates;
    lpLayout       = &lpClassPrivate->mLayoutTable;

    unsigned int luLayoutCap = lpLayout->muCapacity;
    unsigned int luJ = 0;
    while (luJ < luLayoutCap && !lpLayout->mpBuckets[luJ].IsOccupied())
        ++luJ;

    while (luJ < luLayoutCap && lpLayout->mpBuckets[luJ].IsOccupied())
    {
        const u64 luKey = lpLayout->GetKeyAtIndex(luJ);
        const unsigned int luFound = lpLayout->FindIndex(luKey);

        const bool lbValid = luFound < lpLayout->muCapacity &&
                             lpLayout->mpBuckets[luFound].IsOccupied();
        Attrib::Node* lpNode = lbValid
            ? reinterpret_cast<Attrib::Node*>(&lpLayout->mpBuckets[luFound])
            : NULL;

        if (lbValid && lpNode != NULL)
        {
            CGS_ASSERT((lpNode->muFlags & 0x80u) != 0, "Invalid node found at valid index.");

            const u8 lu8Flags = lpNode->muFlags;
            void* lpData;
            if ((lu8Flags & 0x40u) != 0)
            {
                // By-value: the value lives inline in the node (node + 8).
                lpData = reinterpret_cast<u8*>(lpNode) + 8;
            }
            else if ((lu8Flags & 0x10u) != 0)
            {
                // Laid out: an offset into this instance's own data area.
                lpData = reinterpret_cast<u8*>(mpData) + lpNode->muValue;
            }
            else if ((lu8Flags & 0x20u) != 0)
            {
                // Inherited: an offset into the class's inherited data area
                // (X360 classPrivate+0x34 == mStaticData), by name.
                lpData = static_cast<u8*>(lpClassPrivate->mStaticData) + lpNode->muValue;
            }
            else
            {
                // Plain pointer stored in the node's value word.
                lpData = lpNode->mpValue;   // full-width payload
            }

            if (lpData != NULL)
            {
                const bool lbIsArray         = (lu8Flags & 0x2u) != 0;
                const bool lbRequiresRelease = (lu8Flags & 0x1u) != 0;
                const TypeDesc* lpTypeDesc = lpNode->GetTypeDesc();
                FreeNodeData(lbIsArray, lpData, lbRequiresRelease, *lpTypeDesc);
                ++luCleared;
            }
        }
        else
        {
            CGS_ASSERT(false, "Attribute wasn't freed from collection");
        }

        luLayoutCap = lpLayout->muCapacity;
        ++luJ;
        while (luJ < luLayoutCap && !lpLayout->mpBuckets[luJ].IsOccupied())
            ++luJ;
    }

    CGS_ASSERT(luExpected == luCleared,
               "Not all attributes were cleared in Collection::Clear");
}

// ============================================================================
// Attrib::ScanForValidKey<Attrib::HashMap> @ 0x82803CC0
// ============================================================================
// The key cursor every collection walk is built on: advance past luIndex, skip
// free buckets, and hand back the key of the first occupied one (tail-calling
// HashMap::GetKeyAtIndex); 0 when the table has no live bucket left. Callers
// start a fresh walk with luIndex == 0xFFFFFFFF, which the leading increment
// wraps to bucket 0 (the X360 `addi r4,r4,1` on the raw uint).
u64 Attrib::ScanForValidKey(const HashMap& lrMap, unsigned int luIndex)
{
    const unsigned int luCapacity = lrMap.muCapacity;
    ++luIndex;
    while (luIndex < luCapacity && !lrMap.mpBuckets[luIndex].IsOccupied())
        ++luIndex;

    if (luIndex < luCapacity && lrMap.mpBuckets[luIndex].IsOccupied())
        return lrMap.GetKeyAtIndex(luIndex);
    return 0;
}

// ============================================================================
// Attrib::Collection::NextKey @ 0x828050D0
// ============================================================================
// Step the two-phase attribute walk. While lrbInLayoutTable is false the cursor
// is inside THIS collection's own bucket array: re-home luKey, and if it still
// names a live bucket scan on from it. When that phase is exhausted the walk
// flips to the owning class's shared layout table (classPrivate + 0x10) and
// restarts there. In the layout phase the same re-home/scan runs against that
// table; 0 ends the walk.
u64 Attrib::Collection::NextKey(u64 luKey, bool& lrbInLayoutTable) const
{
    if (!lrbInLayoutTable)
    {
        const unsigned int luIndex = FindIndex(luKey);
        if (luIndex < muCapacity && mpBuckets[luIndex].IsOccupied())
        {
            const u64 luNext = ScanForValidKey(*this, luIndex);
            if (luNext != 0)
                return luNext;
        }

        // Own table exhausted -- continue in the class's shared layout table.
        lrbInLayoutTable = true;
        const ClassPrivate* lpClassPrivate =
            reinterpret_cast<const ClassPrivate*>(mpClass)->mpPrivates;
        return ScanForValidKey(lpClassPrivate->mLayoutTable, 0xFFFFFFFFu);
    }

    const ClassPrivate* lpClassPrivate =
        reinterpret_cast<const ClassPrivate*>(mpClass)->mpPrivates;
    const HashMap& lrLayout = lpClassPrivate->mLayoutTable;
    const unsigned int luIndex = lrLayout.FindIndex(luKey);
    if (luIndex < lrLayout.muCapacity && lrLayout.mpBuckets[luIndex].IsOccupied())
        return ScanForValidKey(lrLayout, luIndex);
    return 0;
}

// ============================================================================
// Attrib::Collection::Collection (the LOAD ctor) @ 0x82809740
// ============================================================================
// Build a live collection from one serialised CollectionLoadData export: size the
// attribute table from the load data, bind the collection's identity (key / owning
// class / layout block / source vault), take the references the collection owns
// (itself, its source vault, the class's shared layout table, and -- when the load
// data names a parent -- the parent collection), insert every serialised attribute
// entry into the table, and finally stamp each visible array attribute's header
// with its schema type index.
namespace Attrib
{
    namespace
    {
        // Diagnostic-only census accumulators (X360 dword_83011BB8 / _BBC / _BC0):
        // total probe cache lines, attributes added, by-value payload bytes.
        // Nothing in the runtime reads them back.
        u32 sLoadSearchCacheLines = 0;   // X360 dword_83011BB8
        u32 sLoadAttributesAdded  = 0;   // X360 dword_83011BBC
        u32 sLoadByValueBytes     = 0;   // X360 dword_83011BC0
    }
}

Attrib::Collection::Collection(const CollectionLoadData& lrLoad, Vault* lpSource)
    : HashMap(lrLoad.mTableReserve, static_cast<u8>(lrLoad.mTableKeyShift), 1)
{
    mpParent = NULL;
    mKey     = lrLoad.mKey;

    // Owning class (must already be registered -- ClassExportPolicy runs first).
    DatabasePrivate* lpPrivates = GetDatabasePrivate();
    mpClass  = lpPrivates->mClasses.Find(lrLoad.mClass);
    mpData   = lrLoad.GetLayout();
    mpSource = lpSource;

    AddRef();
    CGS_ASSERT(mpClass != NULL, "Attrib::Class not found for collection.");

    // The collection owns a reference on the vault its serialised data lives in
    // (X360 `++vault->mRefCount` inline).
    mpSource->AddRef(0);

    ClassPrivate* lpClassPrivate = reinterpret_cast<ClassPrivate*>(mpClass)->mpPrivates;
    const bool lbAdded = lpClassPrivate->mCollections.Add(mKey, this);
    CGS_ASSERT(lbAdded, "Failed to add Attrib::Collection.");
    (void)lbAdded;

    // ... and one on the class's shared layout table it resolves inherited
    // attributes through.
    lpClassPrivate->mLayoutTable.AddRef();

    if (lrLoad.mParent != 0)
    {
        mpParent = lpClassPrivate->mCollections.Find(lrLoad.mParent);
        CGS_ASSERT(mpParent != NULL, "Parent collection not found for collection.");
        CGS_ASSERT(mpParent != NULL && mpParent->mpClass == mpClass,
                   "Parent's class doesn't match child's class.");
        if (mpParent != NULL)
            mpParent->AddRef();
    }

    // ---- insert every serialised attribute entry ----------------------------
    const u64* lpTypeKeys = lrLoad.GetTypeKeys();
    const CollectionLoadData::Entry* lpEntries = lrLoad.GetEntries();
    for (u32 luEntry = 0; luEntry < lrLoad.mNumEntries; ++luEntry)
    {
        const CollectionLoadData::Entry& lrEntry = lpEntries[luEntry];
        CGS_ASSERT(lrEntry.muTypeIndex <= lrLoad.mNumTypes,
                   "Invalid type index while loading collection.");

        // By-value attributes (flag 0x40) fold their payload size into the census
        // -- but only for the small types the X360 counts (size <= 4).
        if ((lrEntry.mu8Flags & 0x40u) != 0)
        {
            const TypeDesc& lrDesc =
                Database::Get().GetTypeDesc(lpTypeKeys[lrEntry.muTypeIndex]);
            if (lrDesc.mSize <= 4u)
                sLoadByValueBytes += lrDesc.mSize;
        }

        Add(lrEntry.mKey, lpTypeKeys[lrEntry.muTypeIndex],
            reinterpret_cast<void*>(static_cast<uintptr_t>(lrEntry.muValue)),
            /*lbLaidOut*/ false, lrEntry.mu8Flags, /*lbNoGrow*/ true, mpData);

        sLoadSearchCacheLines += static_cast<u32>(CountSearchCacheLines(lrEntry.mKey, 7));
        ++sLoadAttributesAdded;
    }

    // ---- stamp the type index into every visible array header ---------------
    // Walk every attribute key this collection resolves (own table first, then
    // the class layout table) and, for the array-typed nodes THIS collection
    // owns, write the schema type index into the Array header's packed type word
    // (preserving its top bit).
    bool lbInLayoutTable;
    u64  luKey = ScanForValidKey(*this, 0xFFFFFFFFu);
    if (luKey != 0)
    {
        lbInLayoutTable = false;
    }
    else
    {
        luKey = ScanForValidKey(lpClassPrivate->mLayoutTable, 0xFFFFFFFFu);
        lbInLayoutTable = true;
    }

    for (; luKey != 0; luKey = NextKey(luKey, lbInLayoutTable))
    {
        const Collection* lpContainer = NULL;
        Attrib::Node* lpNode = GetNode(luKey, lpContainer);
        if (lpContainer != this || lpNode == NULL || (lpNode->muFlags & 0x2u) == 0)
            continue;

        // Resolve the node's schema TypeDesc through the database's indexed-type
        // vector (index clamped to 0 when out of range, then bounds-checked --
        // the inlined EASTL vector::operator[]).
        DatabasePrivate* lpDb = GetDatabasePrivate();
        u32 luTypeIndex = lpNode->mTypeIndex;
        if (luTypeIndex >= lpDb->mNumCompiledTypes)
            luTypeIndex = 0;
        CGS_ASSERT(luTypeIndex < lpDb->mCompiledTypes.Size(),
                   "vector::operator[] -- out of range");
        const TypeDesc* lpDesc = lpDb->mCompiledTypes.mpBegin[luTypeIndex];

        // The Array header address, by the node's storage class.
        Array* lpArray;
        if ((lpNode->muFlags & 0x10u) != 0)
            lpArray = reinterpret_cast<Array*>(static_cast<u8*>(mpData) + lpNode->muValue);
        else if ((lpNode->muFlags & 0x20u) != 0)
            lpArray = reinterpret_cast<Array*>(
                static_cast<u8*>(lpClassPrivate->mStaticData) + lpNode->muValue);
        else
            lpArray = reinterpret_cast<Array*>(lpNode->mpValue);   // full-width payload

        lpArray->muTypeInfo = static_cast<u16>((lpArray->muTypeInfo & 0x8000u) |
                                               static_cast<u16>(lpDesc->mIndex));
    }
}
