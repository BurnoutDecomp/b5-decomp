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
        const Vector4 lLineStart = { lrLine.mStart.x, lrLine.mStart.y, lrLine.mStart.z, lrLine.mStart.w };
        const Vector4 lLineEnd   = { lrLine.mEnd.x,   lrLine.mEnd.y,   lrLine.mEnd.z,   lrLine.mEnd.w };

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

                if (!TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lpNode->mBox))
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
