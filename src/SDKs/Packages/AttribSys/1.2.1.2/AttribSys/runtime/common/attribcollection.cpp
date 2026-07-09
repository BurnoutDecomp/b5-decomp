#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribute.h"      // Attrib::Node
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribarray.h"    // Array/TypeDesc/ITypeHandler
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h" // AttribListNode/Base + delete-queue seam
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"   // Attrib::Free

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
// Attribute::GetInternalPointer bodies use. NextKey (@0x828050D0) is NOT reconstructed here:
// it dispatches through the un-homed Attrib::ScanForValidKey<Attrib::HashMap> template
// (mangled ___ScanForValidKey_VHashMap_Attrib___), which has no reconstructed home yet.
//
// A Collection IS-A Attrib::HashMap (attribhashmap.h): the X360 passes a Collection*
// straight into HashMap::Release and reaches the shared refcount at the HashMap +0x08
// slot. AddRef therefore bumps the inherited muRefCount, and Release forwards to the real
// HashMap::Release and -- on the final drop -- queues the collection onto the attribute
// database's garbage list. The scalar deleting destructor (@0x8280C510) is homed beside
// its CollectionHashMap::Clear caller in vechashmap.cpp.

namespace Attrib
{
    // ---- cross-TU trap stubs (own ledger TUs) -------------------------------
    // Read the process attribute database's private impl pointer (off_83011BC4 followed to
    // +4 = mPrivates; asserts the database is initialized). Bodied in attribclassprivate.cpp;
    // declared here so Release compiles without dragging in attribsys.h.
    void* GetDatabasePrivate();

    // Attrib::DatabasePrivate::QueueForDelete<Attrib::Collection> @ 0x8280BEA8. Defer a
    // collection for garbage collection on the database's collection garbage list (an intrusive
    // eastl::list ring, AttribListBase). The X360 asserts the object is non-NULL and unreferenced
    // (its shared HashMap refcount at +0x08 is zero), then -- only if it is not already queued --
    // allocates a node holding the collection and links it at the tail of the ring. Reached from
    // Attrib::Collection::Release on the final refcount drop.
    void* DatabasePrivate_QueueCollectionForDelete(void* lpCollection, void* lpGarbageList)
    {
        AttribListBase* lpQueue = reinterpret_cast<AttribListBase*>(lpGarbageList);

        // X360 reads the collection's shared refcount at +0x08 (the HashMap base's muRefCount).
        bool lbNullOrReferenced = (lpCollection == NULL);
        if (!lbNullOrReferenced)
        {
            const u16 lu16RefCount =
                *reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(lpCollection) + 0x08);
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
        u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
        return static_cast<int>(reinterpret_cast<intptr_t>(
            DatabasePrivate_QueueCollectionForDelete(this, lpPrivates + 0x6C)));
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
Attrib::Node* Attrib::Collection::GetNode(Key luKey, const Collection*& lrpContainer) const
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
        u8* lpClass        = reinterpret_cast<u8*>(mpClass);
        u8* lpClassPrivate = *reinterpret_cast<u8**>(lpClass + 0x08);
        const HashMap* lpLayoutTable =
            reinterpret_cast<const HashMap*>(lpClassPrivate + 0x10);

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
void* Attrib::Collection::GetData(Key luKey, unsigned int luIndex) const
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
            u8* lpClass        = reinterpret_cast<u8*>(lpOwner->mpClass);
            u8* lpClassPrivate = *reinterpret_cast<u8**>(lpClass + 0x08);
            u8* lpDataBase     = *reinterpret_cast<u8**>(lpClassPrivate + 0x34);
            lpArray = reinterpret_cast<Array*>(lpDataBase + lpNode->muValue);
        }
        else
        {
            // Plain: the node's value word is the (X360 32-bit) Array pointer image.
            lpArray = reinterpret_cast<Array*>(static_cast<uintptr_t>(lpNode->muValue));
        }
        return lpArray->GetData(luIndex);
    }

    CGS_ASSERT(luIndex == 0, "Cannot get non-array data from a non-zero index.");
    return lpNode->GetPointer(lpOwner->mpData);
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
            Free(lpData, lrTypeDesc.mIndex, "Attrib::attribute_data");
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
    u8* lpClassPrivate = *reinterpret_cast<u8**>(reinterpret_cast<u8*>(mpClass) + 0x08);
    HashMap* lpLayout  = reinterpret_cast<HashMap*>(lpClassPrivate + 0x10);
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
        // vector (DatabasePrivate: type count @ +0x14, vector<TypeDesc*> begin/end @
        // +0x18/+0x1C), reached by the recovered offsets exactly as Array::GetTypeDesc
        // does. The index is clamped to 0 when out of range, then bounds-checked (the
        // inlined EASTL vector::operator[]).
        u8* lpPrivates = reinterpret_cast<u8*>(GetDatabasePrivate());
        const u32 luNumTypes  = *reinterpret_cast<u32*>(lpPrivates + 0x14);
        u32 luTypeIndex = lpBucket->mTypeIndex;
        if (luTypeIndex >= luNumTypes)
            luTypeIndex = 0;
        TypeDesc** lppBegin = *reinterpret_cast<TypeDesc***>(lpPrivates + 0x18);
        TypeDesc** lppEnd   = *reinterpret_cast<TypeDesc***>(lpPrivates + 0x1C);
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
    lpClassPrivate = *reinterpret_cast<u8**>(reinterpret_cast<u8*>(mpClass) + 0x08);
    lpLayout       = reinterpret_cast<HashMap*>(lpClassPrivate + 0x10);

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
                // Inherited: an offset into the class's inherited data area.
                u8* lpDataBase = *reinterpret_cast<u8**>(lpClassPrivate + 0x34);
                lpData = lpDataBase + lpNode->muValue;
            }
            else
            {
                // Plain pointer stored in the node's value word.
                lpData = reinterpret_cast<void*>(static_cast<uintptr_t>(lpNode->muValue));
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
