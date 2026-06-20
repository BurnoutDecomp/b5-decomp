#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. CgsGeometric::PolygonSoupListSpatialMap:
//   Construct       @ 0x82839600 — zero the handle pointer + active count.
//   Prepare         @ 0x82839610 — adopt a caller-owned handle array + count.
//   Clear           @ 0x82839620 — null the partition/query scratch fields, reset every
//                                   active handle to the invalid (zero) handle, zero the count.
//   AddList         @ 0x82839680 — increment the active count only (argument is unused).
//   GetPolySoupList @ 0x8283AB40 — asserted accessor returning the slot's PolygonSoupList.
//
// Behaviour-faithful to the X360 pseudocode. The spatial partition itself (parent/leaf
// nodes, query buffers) is built/consumed by BuildSpacialPartition/RunQuery in their own
// TUs; only these five lifecycle/accessor functions are reconstructed here.

namespace CgsGeometric
{
    // Construct @0x82839600 — X360: `*result = 0; result[1] = 0;`
    // POD init of just the handle pointer and the active count (the rest is set up by
    // Prepare / BuildSpacialPartition before use).
    void PolygonSoupListSpatialMap::Construct()
    {
        mpaPolySoupListHandles = nullptr;
        miNumSoupLists         = 0;
    }

    // Prepare @0x82839610 — X360: `*result = a2; result[1] = a3;`
    // Adopt a caller-owned handle array (not copied/owned) and record its size.
    void PolygonSoupListSpatialMap::Prepare(CgsResource::ResourceHandle* lpaHandles, s32 liNumSoupLists)
    {
        mpaPolySoupListHandles = lpaHandles;
        miNumSoupLists         = liNumSoupLists;
    }

    // Clear @0x82839620
    // Null only a SPECIFIC subset of fields: the leaf-node array, both query scratch
    // buffers, the output buffer, and the level/size/last-result counts. The parent-node
    // arrays (mapParentNodes/maiParentNodeCounts) and miLeafNodeCount are intentionally
    // NOT touched here — matching the X360, which writes only result[18] and result[20..25].
    // It then resets every active handle to the invalid/zero handle and zeroes the count.
    //
    // The X360 stores the 8-byte global `qword_830393E0` into each handle slot, then sets
    // miNumSoupLists from one of that global's dwords. `qword_830393E0` is the invalid/zero
    // ResourceHandle {0, 0}: both dwords are 0, so this is the standard "reset to empty" —
    // every handle becomes {nullptr, nullptr} and the count becomes 0 (regardless of which
    // dword feeds the count, since the whole handle is zero).
    void PolygonSoupListSpatialMap::Clear()
    {
        mpLeafNodes            = nullptr; // result[18]
        mapQueryBuffers[0]     = nullptr; // result[20]
        mapQueryBuffers[1]     = nullptr; // result[21]
        mpOutputQueryBuffer    = nullptr; // result[22]
        miNumLevels            = 0;       // result[23]
        miQueryBufferSize      = 0;       // result[24]
        miLastQueryResultCount = 0;       // result[25]

        for (s32 li = 0; li < miNumSoupLists; ++li)
        {
            // qword_830393E0 == the invalid/zero ResourceHandle {0, 0}.
            mpaPolySoupListHandles[li].mpResourceMemory = nullptr;
            mpaPolySoupListHandles[li].mpSourceEntry    = nullptr;
        }

        miNumSoupLists = 0; // = the (zero) invalid-handle dword the X360 copies into the count
    }

    // GetPolySoupList @0x8283AB40 (const accessor)
    // X360 deref chain is `**(8*index + mpaPolySoupListHandles)`:
    //   first deref  = handle.mpResourceMemory (the resource memory; committed member),
    //   second deref = the resource's first pointer member (its GetMemoryResource()).
    const PolygonSoupList* PolygonSoupListSpatialMap::GetPolySoupList(s32 liPolySoupListIndex) const
    {
        CGS_ASSERT(liPolySoupListIndex >= 0 && liPolySoupListIndex < miNumSoupLists,
                   "liPolySoupListIndex >= 0 && liPolySoupListIndex < miNumSoupLists");

        const CgsResource::ResourceHandle& lrHandle = mpaPolySoupListHandles[liPolySoupListIndex];
        CGS_ASSERT(lrHandle.mpResourceMemory != nullptr,
                   "lPolySoupListHandle.GetResource() != NULL");

        // GetResource() == mpResourceMemory; GetMemoryResource() == the resource's first
        // pointer member (raw deref of the resource memory).
        void* lpMemoryResource = *reinterpret_cast<void* const*>(lrHandle.mpResourceMemory);
        CGS_ASSERT(lpMemoryResource != nullptr,
                   "lPolySoupListHandle.GetResource()->GetMemoryResource() != NULL");

        return reinterpret_cast<const PolygonSoupList*>(lpMemoryResource);
    }

    // AddList @0x82839680 — X360: `++*(result + 4); return result;`
    // The X360 body ONLY bumps the active count; the PolygonSoupList* argument is genuinely
    // unused here (the handle array is populated elsewhere — Prepare and the resource-load
    // path — and AddList just marks one more slot active). Reproduced faithfully.
    void PolygonSoupListSpatialMap::AddList(const PolygonSoupList* lpPolySoupList)
    {
        (void)lpPolySoupList; // unused — the X360 AddList only bumps the count
        ++miNumSoupLists;
    }
}
