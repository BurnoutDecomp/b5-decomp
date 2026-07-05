#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/SceneManager/TriangleCollision/CgsTriangleCollisionManagerIO_Events.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"

// CgsSceneManager::TriangleCollisionManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Prepare                        @ 0x828B2FF0
//   ProcessAddPolySoupListEvents   @ 0x828B3160

namespace CgsSceneManager
{

// Prepare @0x828B2FF0 — build the triangle-collision scene from a caller-supplied
// linear allocator with a fixed poly-soup-list budget.
//
// X360 store-for-store:
//   * guard the allocator and the budget (KI_MAX_NUM_ZONES == 1024),
//   * carve a mpaPolySoupListHandles[liMaxNumPolySoupLists] array out of lpAllocator
//     (8 bytes/handle on the X360) and fill it with the invalid/null handle,
//   * hand that array (starting empty, count 0) to the spatial map,
//   * register the debug overlay component,
//   * carve the remaining free memory into a private 16-byte-aligned sub-allocator
//     (mSpacialAllocator) that the spatial-partition build later draws from.
// Returns true.
bool TriangleCollisionManager::Prepare(CgsMemory::LinearMalloc* lpAllocator, s32 liMaxNumPolySoupLists)
{
    CGS_ASSERT(lpAllocator != nullptr, "lpAllocator != NULL");
    CGS_ASSERT(liMaxNumPolySoupLists >= 0 && liMaxNumPolySoupLists < KI_MAX_NUM_ZONES,
               "liMaxNumPolySoupLists >= 0 && liMaxNumPolySoupLists < KI_MAX_NUM_ZONES");

    miMaxNumSoupLists = liMaxNumPolySoupLists;

    // Inlined MallocArray<CgsResource::ResourceHandle>: 8 bytes/handle on the X360.
    mpaPolySoupListHandles = static_cast<CgsResource::ResourceHandle*>(
        lpAllocator->Malloc(sizeof(CgsResource::ResourceHandle) * static_cast<size_t>(liMaxNumPolySoupLists)));
    CGS_ASSERT(mpaPolySoupListHandles != nullptr, "mpaPolySoupListHandles != NULL");

    for (s32 liZone = 0; liZone < miMaxNumSoupLists; ++liZone)
    {
        // X360 stores the 8-byte qword_83085AC4 == CgsResource::NULLResourceHandle {0,0}.
        mpaPolySoupListHandles[liZone] = CgsResource::NULLResourceHandle;
    }

    // Start the spatial map empty (count 0) over the freshly-cleared handle array.
    mPolySoupListSpacialMap.Prepare(mpaPolySoupListHandles, 0);

    mDebugComponent.Register();

    mSpacialAllocator.Construct();
    u32 luFreeMemory = (static_cast<u32>(lpAllocator->GetFreeMemory()) + 15u) & 0xFFFFFFF0u;
    CGS_ASSERT(luFreeMemory > 16, "luFreeMemory > 16");

    void* lpSpacialMemory = lpAllocator->Malloc(luFreeMemory);
    mSpacialAllocator.Create(lpSpacialMemory, luFreeMemory);
    mSpacialAllocator.SetAlignment(16);

    return true;
}

// ProcessAddPolySoupListEvents @0x828B3160 — drain the add-poly-soup-list input queue.
//
// X360 store-for-store: for each queued InEventAddPolySoupList, take the next free
// handle slot (asserting it is still the null handle), copy the event's resource handle
// into it, register the list with the spatial map, and accumulate whether any event
// asked for a spatial-partition rebuild. If any did, reset the spatial allocator and
// (re)build the partition (8 lists/node cap, 2048-item budget).
void TriangleCollisionManager::ProcessAddPolySoupListEvents(
    const InAddPolySoupListQueue& lAddPolySoupListQueue)
{
    CGS_ASSERT(mpaPolySoupListHandles != nullptr, "mpaPolySoupListHandles != NULL");

    bool lbNeedToRebuild = false;
    for (s32 liEventIndex = 0; liEventIndex < lAddPolySoupListQueue.GetLength(); ++liEventIndex)
    {
        const CgsSceneManager::TriangleCollisionManagerIO::InEventAddPolySoupList& lEvent =
            lAddPolySoupListQueue.GetEvent(liEventIndex);

        CGS_ASSERT(miNumSoupListsAdded < miMaxNumSoupLists,
                   "miNumSoupListsAdded < miMaxNumSoupLists");
        CGS_ASSERT(mpaPolySoupListHandles[miNumSoupListsAdded] == CgsResource::NULLResourceHandle,
                   "mpaPolySoupListHandles[miNumSoupListsAdded] == CgsResource::NULLResourceHandle");

        mpaPolySoupListHandles[miNumSoupListsAdded] = lEvent.mPolySoupListHandle;

        // AddList ignores its argument (only bumps the map's active count); the X360 still
        // computes the PolygonSoupList* from the freshly-stored handle and passes it.
        mPolySoupListSpacialMap.AddList(
            *reinterpret_cast<const CgsGeometric::PolygonSoupList* const*>(
                mpaPolySoupListHandles[miNumSoupListsAdded].mpResourceMemory));

        ++miNumSoupListsAdded;
        lbNeedToRebuild = lbNeedToRebuild || lEvent.mbRebuildSpacialPartitioning;

        CGS_ASSERT(miNumSoupListsAdded == mPolySoupListSpacialMap.GetNumPolySoupLists(),
                   "miNumSoupListsAdded == mPolySoupListSpacialMap.GetNumPolySoupLists()");
    }

    if (lbNeedToRebuild)
    {
        mSpacialAllocator.FreeAll();
        mPolySoupListSpacialMap.BuildSpacialPartition(&mSpacialAllocator, 8, 2048);
    }
}

}
