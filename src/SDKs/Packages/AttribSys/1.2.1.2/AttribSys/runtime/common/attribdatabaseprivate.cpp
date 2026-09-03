#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabaseprivate.h"

#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // shared AttribSys census-free
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h"           // AttribListNode/Base + node-free seam
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"           // Attrib::Collection (~Collection)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX (AttribSys v1.2.1.2).
//   Attrib::DatabasePrivate::`scalar deleting destructor' @ 0x8280CA40
//
// The X360 thunk is MSVC's scalar deleting destructor for the vtbl'd DatabasePrivate
// registry impl (the private object behind Attrib::Database; DWARF: struct
// Attrib::DatabasePrivate : public Database, sizeof == 172 / 0xAC). It runs the real
// virtual ~DatabasePrivate() (bl @ 0x8280CA5C, a separate AttribSys TU), then -- when the
// low should-free bit of the deleting flag is set -- decrements the shared AttribSys
// live-byte census by 172, refreshes the peak, NULL-checks the block (cmplwi r30,0 / beq
// at the X360 site), and only then frees the 172-byte object through the AttribSys package
// allocator with a NULL diagnostic tag, returning this. Because THIS site null-guards the
// free (unlike the sibling Database @ 0x828057A8 / ExportPolicy sites, which free
// unconditionally via FreeWithCensus), the free is routed through the null-guarded
// FreeWithCensusIf helper; the census decrement itself is unconditional inside the
// should-free branch. The compiler synthesises the deleting thunk from the virtual
// ~DatabasePrivate() plus the class operator delete bodied below.
namespace Attrib
{
    // ~DatabasePrivate @ 0x8280CA5C (inner dtor, called by the deleting thunk). Its real
    // body -- releasing the class registry / garbage queues this object owns -- lives in
    // its own AttribSys TU (todo) and is not part of this batch; bodied empty here so the
    // deleting-destructor thunk this batch reconstructs has a virtual dtor to synthesise
    // from without fabricating the member cleanup.
    DatabasePrivate::~DatabasePrivate()
    {
    }

    // operator delete @ the deleting-destructor's free site. Decrements the shared
    // AttribSys live-byte census by sizeof(DatabasePrivate) (=172), refreshes the peak,
    // then -- only when the block is non-NULL -- frees it through the AttribSys package
    // allocator with a NULL diagnostic tag (GetAttribS()->Free(this, 172, 0)). Routed
    // through the shared null-guarded census-free helper (matching the cmplwi r30,0 / beq
    // the X360 emits before the free) so the two census counters (dword_83011BFC /
    // dword_83011BF8) stay defined exactly once.
    void DatabasePrivate::operator delete(void* lpBlock, size_t lnBytes)
    {
        HashMapTablePolicy::FreeWithCensusIf(lpBlock, lnBytes, NULL);
    }

    // Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Collection> @ 0x8280E3C0. Drain the
    // database's deferred-collection garbage list (an intrusive eastl::list ring, AttribListBase):
    // for each queued node, when the collection it holds is now unreferenced (its shared HashMap
    // refcount at +0x08 is zero) run its destructor and hand the 40-byte object back to the
    // AttribSys package allocator through the shared unconditional census-free; then unlink the
    // node from the ring and free the node itself. Reached from Attrib::Database::CollectGarbage.
    // Homed here as the DatabasePrivate free-function seam (matching the DatabasePrivate_Queue*
    // ForDelete convention); the Class twin (@own TU) frees its nodes the same way.
    AttribListBase* DatabasePrivate_CollectCollectionGarbageBag(AttribListBase* lpGarbageList)
    {
        while (lpGarbageList->mNode.mpNext != &lpGarbageList->mNode)
        {
            AttribListNode*     lpNode       = lpGarbageList->mNode.mpNext;
            Attrib::Collection* lpCollection = reinterpret_cast<Attrib::Collection*>(lpNode->mpValue);

            CGS_ASSERT(lpCollection != NULL, "NULL object found in garbage bag.");

            // X360 reads the collection's shared refcount at +0x08 (HashMap base's muRefCount).
            // ⚠️ [FLAG PC known defect, 2026-09-03 lane A10] on x64 +0x08 is HashMap::muCapacity, NOT the refcount:
            // only EMPTY collections reach the destructor. Reading GetRefCount() by name is the fix, but it must
            // wait for attribhashmap.cpp's Remove/Transfer/UpdateSearchLength (still console byte offsets) --
            // Collection::Clear phase 1 would be their first live caller and would corrupt the buckets.
            const u16 lu16RefCount =
                *reinterpret_cast<const u16*>(reinterpret_cast<const u8*>(lpCollection) + 0x08);
            if (lu16RefCount == 0)
            {
                lpCollection->~Collection();
                // Unconditional census-and-free of the 40-byte collection object (already inside
                // the should-free branch; the object is non-NULL, so no null guard is emitted).
                // (host size, 2026-09-03: the console literal 40 is the X360 sizeof; the x64 Collection is 48.)
                HashMapTablePolicy::FreeWithCensus(lpCollection, sizeof(Attrib::Collection), NULL);
            }

            // Unlink the (first) node from the ring and free it.
            lpNode->mpPrev->mpNext = lpNode->mpNext;
            lpNode->mpNext->mpPrev = lpNode->mpPrev;
            AttribListFreeNode(lpGarbageList, lpNode);
        }
        return lpGarbageList;
    }
}
