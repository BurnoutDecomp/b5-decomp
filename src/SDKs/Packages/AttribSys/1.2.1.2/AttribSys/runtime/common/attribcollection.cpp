#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

#include <cstdint> // intptr_t
#include "GameShared/GameClasses/Core/CgsAssert.h"

// AttribSys runtime -- Attrib::Collection refcount bodies, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2):
//   Attrib::Collection::AddRef  @ 0x828028E0
//   Attrib::Collection::Release @ 0x8280C2E8
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

    // Attrib::DatabasePrivate::QueueForDelete<Attrib::Collection> @ own AttribSys TU (todo):
    // defers a collection for garbage collection on the database's collection garbage list.
    // Trap stub until it lands, matching the sibling DatabasePrivate_QueueClassForDelete.
    void* DatabasePrivate_QueueCollectionForDelete(void* lpCollection, void* lpGarbageList)
    {
        (void)lpCollection; (void)lpGarbageList;
        __debugbreak();
        return NULL;
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
