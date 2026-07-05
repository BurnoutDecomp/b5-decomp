#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribdatabase.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"     // GetEaStlAllocator()
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"  // AttribSysPackageAllocator::Free(void*,s32,const char*)

#include <cstring>   // memcpy (AttribVectorReserve)

// =============================================================================
// AttribSys database container helpers -- reconstructed from BURNOUT_X360_ARTIST.XEX.
// These are the eastl::list / eastl::vector<AttribSysPackageAllocator> methods the X360
// emits under truncated symbols; DWARF (EASTL/list.h, EASTL/vector.h) resolves them.
//
// The list frees route through the STATIC EASTL AttribSys package allocator that the
// memory manager hands out (GetEaStlAllocator()), reproducing the inlined
// sbHasLinearAllocator assert (in the accessor) + mbHasAllocator assert + heap-forward +
// miFreeTotal += 12 (the tagged 3-arg Free) in the correct order. GetEaStlAllocator() and
// the 3-arg tagged Free are declared-only cross-TU seams (bodied in their own homes); they
// link fine under the compile-only gate. The vector helpers DoAllocate/DoFree are likewise
// separate cross-TU bl targets (own TUs).
// =============================================================================

namespace Attrib
{

// @ 0x828054B0 -- eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoClear()
// (DWARF list.h:290). Walk the ring from the first real node (mNode.mpNext) back to the
// sentinel (&mNode), freeing each node through the static EASTL AttribSys package
// allocator. Called by Attrib::DatabasePrivate::~DatabasePrivate.
AttribListBase* AttribListClearNodes(AttribListBase* lpList)
{
    AttribListNode* lpSentinel = &lpList->mNode;
    for (AttribListNode* lpNode = lpList->mNode.mpNext; lpNode != lpSentinel; )
    {
        AttribListNode* lpDoomed = lpNode;
        lpNode = lpNode->mpNext;

        CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Free(
            lpDoomed, KI_ATTRIB_LIST_NODE_SIZE, NULL);
    }
    return lpList;
}

// @ 0x82803FC8 -- eastl::list<const Attrib::Class*, AttribSysPackageAllocator>::DoFreeNode
// (DWARF list.h:284). Hand one detached node back to the static EASTL AttribSys package
// allocator. X360 receives (a1 = list this [unused], a2 = node); only the node is forwarded
// to the same inlined free as DoClear. Unconditional (no null-guard). Called by
// Attrib::DatabasePrivate::CollectGarbageBag<Attrib::Class>.
void AttribListFreeNode(void* /*lpUnusedList*/, void* lpNode)
{
    CgsAttribSys::AttribSysMemoryManager::GetEaStlAllocator()->Free(
        lpNode, KI_ATTRIB_LIST_NODE_SIZE, NULL);
}

// @ 0x8280C258 -- eastl::vector<const Attrib::TypeDesc*, AttribSysPackageAllocator>::reserve(n)
// (DWARF vector.h:126; instantiated for Attrib::TypeDescPtrVec). If n exceeds the current
// capacity ((mpCapacityEnd - mpBegin) elements), allocate an n-element buffer, copy the live
// [mpBegin, mpEnd) bytes across, free the old buffer (by its old element capacity), then
// repoint {mpBegin, mpCapacityEnd, mpEnd}. Otherwise a no-op. Called by
// Attrib::DatabasePrivate::DatabasePrivate.
AttribVectorBase* AttribVectorReserve(AttribVectorBase* lpVector, unsigned int luCapacity)
{
    AttribVectorBase* lpSelf     = lpVector;
    void**            lpOldBegin = lpVector->mpBegin;

    const s32 liCurrentCapacityElems =
        static_cast<s32>(reinterpret_cast<u8*>(lpVector->mpCapacityEnd) -
                         reinterpret_cast<u8*>(lpVector->mpBegin)) >> 2;

    if (luCapacity > static_cast<unsigned int>(liCurrentCapacityElems))
    {
        void** lpOldEnd = lpVector->mpEnd;

        void** lpNewBuffer =
            static_cast<void**>(AttribVectorAllocate(lpVector, luCapacity));

        memcpy(lpNewBuffer, lpOldBegin,
               static_cast<size_t>(reinterpret_cast<u8*>(lpOldEnd) -
                                   reinterpret_cast<u8*>(lpOldBegin)));

        // Free the old buffer by its still-live element capacity (recomputed from
        // begin/capacityEnd @ 0x8280C29C-0x8280C2B0).
        const s32 liOldCapacityElems =
            static_cast<s32>(reinterpret_cast<u8*>(lpVector->mpCapacityEnd) -
                             reinterpret_cast<u8*>(lpVector->mpBegin)) >> 2;
        AttribVectorFree(lpSelf, liOldCapacityElems);

        // Live-element count recovered from old begin/end captured (r9/r10) after the
        // free but before the mpBegin store.
        void** lpPrevBegin = lpSelf->mpBegin;
        void** lpPrevEnd   = lpSelf->mpEnd;
        const s32 liLiveElems =
            static_cast<s32>(reinterpret_cast<u8*>(lpPrevEnd) -
                             reinterpret_cast<u8*>(lpPrevBegin)) >> 2;

        lpSelf->mpBegin       = lpNewBuffer;
        lpSelf->mpCapacityEnd = lpNewBuffer + luCapacity;
        lpSelf->mpEnd         = lpNewBuffer + liLiveElems;
    }
    return lpSelf;
}

} // namespace Attrib
