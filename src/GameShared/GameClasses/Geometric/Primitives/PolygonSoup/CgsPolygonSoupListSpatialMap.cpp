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
        mpaPolySoupListHandles = 0;
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
        mpLeafNodes            = 0; // result[18]
        mapQueryBuffers[0]     = 0; // result[20]
        mapQueryBuffers[1]     = 0; // result[21]
        mpOutputQueryBuffer    = 0; // result[22]
        miNumLevels            = 0;       // result[23]
        miQueryBufferSize      = 0;       // result[24]
        miLastQueryResultCount = 0;       // result[25]

        for (s32 li = 0; li < miNumSoupLists; ++li)
        {
            // qword_830393E0 == the invalid/zero ResourceHandle {0, 0}.
            mpaPolySoupListHandles[li].mpResourceMemory = 0;
            mpaPolySoupListHandles[li].mpSourceEntry    = 0;
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
        CGS_ASSERT(lrHandle.mpResourceMemory != 0,
                   "lPolySoupListHandle.GetResource() != NULL");

        // GetResource() == mpResourceMemory; GetMemoryResource() == the resource's first
        // pointer member (raw deref of the resource memory).
        void* lpMemoryResource = *reinterpret_cast<void* const*>(lrHandle.mpResourceMemory);
        CGS_ASSERT(lpMemoryResource != 0,
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

    // GetNumLeafNodes @0x82917018 — X360: `return *(a1 + 0x4C);`
    //   stw r3,arg; lwz r11,arg; lwz r3,0x4C(r11); blr
    // +0x4C is miLeafNodeCount (result[19]); a plain word-load accessor.
    s32 PolygonSoupListSpatialMap::GetNumLeafNodes() const
    {
        return miLeafNodeCount;
    }

    // GetPolygonSoup @0x8280FFD0 — X360: `return *(a1 + 0x48) + 0x30 * a2;`
    //   slwi r10,r4,1 ; add r10,r4,r10 (=> 3*index) ; slwi r10,r10,4 (=> 48*index)
    //   lwz r11,0x48(r3) ; add r3,r11,r10 ; blr
    // Base pointer is mpLeafNodes (+0x48); the asm element stride is 0x30 (48) bytes.
    // PolygonSoupLeafNode is a declared-only element here, so the stride is honoured by
    // an explicit byte step over the (opaque) element (opaque-element-stride precedent).
    PolygonSoupLeafNode* PolygonSoupListSpatialMap::GetPolygonSoup(s32 liIndex) const
    {
        static const u32 KU_LEAF_NODE_STRIDE = 0x30; // asm: 48 * index (3<<4)
        u8* lpBase = reinterpret_cast<u8*>(mpLeafNodes);
        return reinterpret_cast<PolygonSoupLeafNode*>(lpBase + KU_LEAF_NODE_STRIDE * static_cast<u32>(liIndex));
    }
}

// FOLDED FROM CgsPolygonSoupListSpatialMap_wBT_01.cpp
// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/
// CgsPolygonSoupListSpatialMap_wBT_01.cpp
//
// CgsGeometric::PolygonSoupListSpatialMap::RunJobQuery(const Line&, ...) -- the const,
// job-side SEGMENT query. Its one caller is PolygonSoupTesterJob::RunLineQuery (the
// streamed line-vs-world test); the declaration names it and types all five parameters
// (CgsPolygonSoupListSpatialMap.cpp, the box overload's sibling, `void`, const).
//
// It is RunQuery(const Line&) re-pointed at caller-owned storage, exactly as the box
// RunJobQuery is RunQuery(const AxisAlignedBox&) re-pointed:
//   * the ping/pong buffers and their capacity come from the params block, the answer
//     goes out through the two out-params, and the map's own query fields are untouched;
//   * every level's node array is reached through the caller's ReadOnlyObjectCache
//     (Construct per level, Get per node -- both inlined, with the container's own
//     asserts);
//   * the per-node test is TestLineStartEndAxisAlignedBox inlined whole (the same
//     130-instruction slab sequence as RunQuery(const Line&), register for register,
//     with its three "Line reciprocal X/Y/Z is 0" tripwires);
//   * a node with no index list contributes nothing, checked before the capacity test;
//     the capacity test is (u16)count < (u16)params->miQueryBufferSize, and its assert
//     is CgsPolygonSoupListSpatialMap.cpp (the console streams "level N of M" into
//     the message);
//   * with no levels the two out-params are cleared and control FALLS THROUGH: the
//     seeding still runs and the tail publishes buffer A with count 1, the same as the
//     box overload.
// ============================================================================

#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"            // Line
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"    // TestLineStartEndAxisAlignedBox (inlined per node)

namespace CgsGeometric
{
    void PolygonSoupListSpatialMap::RunJobQuery(
        const Line&                                                 lrLine,
        PolygonSoupJobQueryParams*                                  lpParams,
        u16**                                                       lppuOutResultBuffer,
        s32*                                                        lpiOutNumResults,
        CgsContainers::ReadOnlyObjectCache<PolygonSoupSpacialNode>* lpSpacialNodeCache) const
    {
        if (miNumLevels == 0)
        {
            *lppuOutResultBuffer = NULL;
            *lpiOutNumResults    = 0;
        }

        u16* lpuSrcNodeIndices  = lpParams->mpaQueryBufferA;
        u16* lpuDestNodeIndices = lpParams->mpaQueryBufferB;

        // The segment's two 16-byte lanes, w included.
        const Vector3 lLineStart = lrLine.mStart.GetVector3();
        const Vector3 lLineEnd = lrLine.mEnd.GetVector3();

        // Seed: the single root node of level 0.
        lpuSrcNodeIndices[0] = 0;
        u16 luNumSrcNodeIndices = 1;

        for (s32 liLevel = 0; liLevel < miNumLevels; ++liLevel)
        {
            const PolygonSoupSpacialNode* lpNodes = mapParentNodes[liLevel];
            lpSpacialNodeCache->Construct(lpNodes, maiParentNodeCounts[liLevel], 0, 1);

            u16 luNumDestNodeIndices = 0;

            for (u16 luNodeIndexEntry = 0; luNodeIndexEntry < luNumSrcNodeIndices; ++luNodeIndexEntry)
            {
                const PolygonSoupSpacialNode* lpNode =
                    lpSpacialNodeCache->Get(static_cast<s32>(lpuSrcNodeIndices[luNodeIndexEntry]));

                if (!TestLineStartEndAxisAlignedBox(lpNode->mBox, lLineStart, lLineEnd).GetBool())
                {
                    continue;
                }

                const u16* lpNodeIndices = lpNode->mpaIndices;

                for (u16 luNodeToAdd = 0; luNodeToAdd < lpNode->mu16NumIndices; ++luNodeToAdd)
                {
                    //  -- the console streams "level N of M" into the message; CGS_ASSERT
                    // takes a literal. Both sides are compared as u16.
                    CGS_ASSERT(luNumDestNodeIndices < static_cast<u16>(lpParams->miQueryBufferSize),
                               "Too many results in level ");

                    lpuDestNodeIndices[luNumDestNodeIndices] = lpNodeIndices[luNodeToAdd];
                    ++luNumDestNodeIndices;
                }
            }

            // lpTemp: ping <-> pong, and the next level starts from what this one wrote.
            u16* lpTemp         = lpuSrcNodeIndices;
            lpuSrcNodeIndices   = lpuDestNodeIndices;
            lpuDestNodeIndices  = lpTemp;
            luNumSrcNodeIndices = luNumDestNodeIndices;
        }

        // The buffer the last level WROTE (the swap has already happened), and its count.
        *lppuOutResultBuffer = lpuSrcNodeIndices;
        *lpiOutNumResults    = static_cast<s32>(luNumSrcNodeIndices);
    }
}
