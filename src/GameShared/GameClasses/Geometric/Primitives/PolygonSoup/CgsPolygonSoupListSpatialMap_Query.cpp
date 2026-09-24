// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/PolygonSoup/
// CgsPolygonSoupListSpatialMap_Query.cpp
//
// CgsGeometric::PolygonSoupListSpatialMap::RunJobQuery -- the const, job-side box
// query. Reconstructed from BURNOUT_X360_ARTIST.XEX @0x82844680 (316 instructions).
//
// ⚠️ THE X360 SYMBOL IS A HOLE: there is no name for 0x82844680 in any of the 30,084
// export JSONs. Recovered by the standing technique (the name lives in the neighbour
// set, and the PS3 DWARF types the signature):
//   * `xrefs_to` on 0x82844680 is exactly ONE caller, PolygonSoupTesterJob::RunBoxQuery
//     @0x82916D28, which passes (map, aabb, {ping,pong,2048}, &out, &count, cache);
//   * the PS3 mangle
//       _ZNK12CgsGeometric25PolygonSoupListSpatialMap11RunJobQueryE
//         RKNS_14AxisAlignedBoxEPNS_25PolygonSoupJobQueryParamsEPPtPiP
//         N13CgsContainers19ReadOnlyObjectCacheINS_22PolygonSoupSpacialNodeEEE  @0xB63F20
//     names it and types all five parameters + the const;
//   * its baked assert line is CgsPolygonSoupListSpatialMap.cpp:614, i.e. this class's
//     own file -- the sibling RunQuery @0x82843A80 asserts at :447 in the same file.
//
// ⚠️ THIS IS NOT `RunQuery`. Three prior costings of the triangle-cache fill leg named
// RunQuery @0x82843A80 (261) as the query the worker runs. It is not: RunQuery ping-pongs
// through the map's OWN mapQueryBuffers and publishes into mpOutputQueryBuffer /
// miLastQueryResultCount, which a job holding a read-only copy of the map cannot do.
// RunBoxQuery calls THIS one.
//
// The traversal is a breadth-first level sweep. Level 0 starts from the single root node
// index 0; each level intersects the query box against every candidate node of that level
// and appends that node's index list to the other buffer; the two buffers ping-pong. After
// the last level the surviving indices are LEAF indices, which is what FillTriangleCache
// then walks.
//
// ⭐ VMX LOWERING: the box overlap is six `vcmpgefp` lanes reduced by two `vpermwi128`
// shuffles (imm 0x4B == lanes [1,0,2,3] and 0x87 == [2,0,1,3]) and a `vspltw 0`, so lane 0
// of the reduction is `v0[0] & v0[1] & v0[2]` -- the w lane is DELIBERATELY EXCLUDED, which
// matches CgsAxisAlignedBox.h's "xyz used, w lane spare". Lowered to portable scalar float
// comparisons over x/y/z only, per this project's Triangle4/Line precedent.
// ============================================================================

#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"              // Line (RunQuery(const Line&))
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"      // TestLineStartEndAxisAlignedBox (inlined per node)

namespace CgsGeometric
{
    namespace
    {
        // 0x82844884..0x828448B4. `vcmpgefp v13, queryMax, nodeMin` and
        // `vcmpgefp v0, nodeMax, queryMin`, ANDed, then reduced over x/y/z.
        inline bool BoxesOverlapXYZ(const AxisAlignedBox& lrA, const AxisAlignedBox& lrB)
        {
            if (!(lrA.mMax.x >= lrB.mMin.x)) return false;
            if (!(lrA.mMax.y >= lrB.mMin.y)) return false;
            if (!(lrA.mMax.z >= lrB.mMin.z)) return false;
            if (!(lrB.mMax.x >= lrA.mMin.x)) return false;
            if (!(lrB.mMax.y >= lrA.mMin.y)) return false;
            if (!(lrB.mMax.z >= lrA.mMin.z)) return false;
            return true;
        }
    }

    // ------------------------------------------------------------------------
    // RunJobQuery @0x82844680 (316)
    //
    //   0x828446C0  lwz  r11, 0x5C(this)          -> miNumLevels
    //   0x828446D4  stw  r24, 0(r6) / 0(r7)       -> the zero-levels arm CLEARS BOTH
    //                                                out-params and then FALLS THROUGH
    //   0x828446E4  lwz  r11, 0(r5)               -> params->mpaQueryBufferA  (ping)
    //   0x828446DC  lwz  r10, 4(r5)               -> params->mpaQueryBufferB  (pong)
    //   0x828446F0  sth  r24, 0(r11)              -> ping[0] = 0  (16-bit: root index)
    //   0x828446E0  li   r29, 1                   -> count = 1
    //   0x82844700  ble  loc_82844B44             -> and skips the sweep entirely
    //   0x82844708  addi r25, this, 0x28          -> cursor over maiParentNodeCounts[]
    //   0x828447AC  lwz  r31, -0x20(r25)          -> mapParentNodes[level]  (0x28-0x20 == 0x08)
    //   0x828447B0  lwz  r30,  0x00(r25)          -> maiParentNodeCounts[level]
    //   0x82844B20  addi r8, r25, 4               -> ++level cursor
    //   0x82844B44  stw  r11, 0(r6) / stw r10, 0(r7)
    // ------------------------------------------------------------------------
    s32 PolygonSoupListSpatialMap::RunJobQuery(
        const AxisAlignedBox&                                       lrQueryBox,
        PolygonSoupJobQueryParams*                                  lpParams,
        u16**                                                       lppaOutResults,
        s32*                                                        lpiOutNumResults,
        CgsContainers::ReadOnlyObjectCache<PolygonSoupSpacialNode>* lpNodeCache) const
    {
        // 0x828446CC/0x828446D0: with no partition there is nothing to sweep. The console
        // writes both out-params here and then FALLS THROUGH into the seeding -- the loop
        // is skipped a few instructions later by its own `ble`, and the tail overwrites the
        // out-params with the (still-seeded) ping buffer and count. Reproduced as written:
        // the early store is dead on every path that reaches the tail, but it is what the
        // console does and it is what a caller that ignores the return value observes.
        if (miNumLevels == 0)
        {
            *lppaOutResults   = NULL;
            *lpiOutNumResults = 0;
        }

        u16* lpaPing = lpParams->mpaQueryBufferA;
        u16* lpaPong = lpParams->mpaQueryBufferB;

        // Seed: the sweep starts at the single root node of level 0.
        lpaPing[0] = 0;
        u16 lu16NumCandidates = 1;

        if (miNumLevels > 0)
        {
            for (s32 liLevel = 0; liLevel < miNumLevels; ++liLevel)
            {
                PolygonSoupSpacialNode* lpaLevelNodes = mapParentNodes[liLevel];
                const s32               liLevelCount  = maiParentNodeCounts[liLevel];

                // 0x828447B4..0x828447F0 -- ReadOnlyObjectCache::Construct, INLINED here.
                // Both baked asserts are the container's own
                // (..\..\..\GameShared\GameClasses\Containers/CgsReadOnlyObjectCache.h).
                CGS_ASSERT(lpaLevelNodes != NULL, "Source data is NULL\n");            // :150
                CGS_ASSERT(liLevelCount >= 0, "Source data count must be positive\n"); // :151
                lpNodeCache->Construct(lpaLevelNodes, liLevelCount, 0, 1);

                const u16 lu16CandidatesThisLevel = lu16NumCandidates;
                lu16NumCandidates = 0;

                // 0x82844804/0x8284480C: an empty level short-circuits straight to the swap.
                if (lu16CandidatesThisLevel != 0)
                {
                    for (u32 luCandidate = 0; luCandidate < lu16CandidatesThisLevel; ++luCandidate)
                    {
                        const u16 lu16NodeIndex = lpaPing[luCandidate];

                        // 0x8284482C..0x82844854 -- ReadOnlyObjectCache::Get's bounds
                        // assert, also inlined (CgsReadOnlyObjectCache.h:265).
                        CGS_ASSERT(lu16NodeIndex < liLevelCount, "Index out of range\n");

                        const PolygonSoupSpacialNode& lrNode =
                            *lpNodeCache->Get(static_cast<s32>(lu16NodeIndex));

                        if (!BoxesOverlapXYZ(lrQueryBox, lrNode.mBox))
                        {
                            continue;   // 0x828448B4
                        }

                        // 0x828448B8/0x828448BC: a node with no index list contributes
                        // nothing (checked BEFORE the capacity test, so an empty node can
                        // never trip the overflow assert).
                        if (lrNode.mu16NumIndices == 0)
                        {
                            continue;
                        }

                        for (u32 luIndex = 0; luIndex < lrNode.mu16NumIndices; ++luIndex)
                        {
                            // 0x828448CC..0x828448E0: the capacity is the PARAMS' buffer
                            // size (NOT the map's miQueryBufferSize -- this overload never
                            // touches the map's own buffers), compared as 16-bit.
                            CGS_ASSERT(lu16NumCandidates <
                                           static_cast<u16>(lpParams->miQueryBufferSize),
                                       "Too many results in level ");   // .cpp:614

                            lpaPong[lu16NumCandidates] = lrNode.mpaIndices[luIndex];
                            ++lu16NumCandidates;
                        }
                    }
                }

                // 0x82844B08..0x82844B30: ping <-> pong, then the next level's node array.
                u16* lpaSwap = lpaPing;
                lpaPing      = lpaPong;
                lpaPong      = lpaSwap;
            }
        }

        // 0x82844B40..0x82844B4C. lpaPing is the buffer the last level WROTE (the swap has
        // already happened), and the count is returned 16-bit-clamped (`clrlwi r10, r29, 16`).
        *lppaOutResults   = lpaPing;
        *lpiOutNumResults = static_cast<s32>(lu16NumCandidates);

        return static_cast<s32>(lu16NumCandidates);
    }

    // ------------------------------------------------------------------------
    // RunQuery @0x82843A80 (261) -- scene-query wave 1b, 2026-09-02.
    //
    // The synchronous twin of RunJobQuery above, read off the same shape:
    //   0x82843AAC  lwz  r11, 0x5C(this)   -> miNumLevels; zero -> `li r3, 0`, no writes
    //               ping = mapQueryBuffers[0] (+0x50), pong = mapQueryBuffers[1] (+0x54);
    //               `sth 0 -> ping[0]`, count = 1
    //               cursor over mapParentNodes[] from this+8;
    //               node = level + 48*idx  (`16*(x + rol(x,1))` == 48x, the console
    //                                       sizeof(PolygonSoupSpacialNode)). NO
    //                                       ReadOnlyObjectCache here, unlike RunJobQuery,
    //                                       and so none of its asserts.
    //               six vcmpgefp lanes, vpermwi 0x4B/0x87, vspltw 0 -> BoxesOverlapXYZ,
    //               then `lhz 0x24(node)` != 0 before any capacity test
    //   :447        "Too many results in level %d of %d / BoxMin: x y z / BoxMax: x y z" (streamed)
    //               when count >= miQueryBufferSize (+0x60), compared 32-bit
    //   tail        ping<->pong per level; mpOutputQueryBuffer (+0x58) = the buffer the last
    //               level wrote, miLastQueryResultCount (+0x64) = count; return count (u16)
    //
    // BuildSpacialPartition never writes mpOutputQueryBuffer (its own FLAG at
    // CgsPolygonSoupListSpatialMap_Build.cpp:453); this is the function that does.
    // ------------------------------------------------------------------------
    s32 PolygonSoupListSpatialMap::RunQuery(const AxisAlignedBox& lrQueryBox)
    {
        if (miNumLevels == 0)
        {
            return 0;   // the `li r3, 0` exit; the output fields are untouched
        }

        u16* lpaPing = mapQueryBuffers[0];
        u16* lpaPong = mapQueryBuffers[1];

        // Seed: the single root node of level 0.
        lpaPing[0] = 0;
        u16 lu16NumCandidates = 1;

        for (s32 liLevel = 0; liLevel < miNumLevels; ++liLevel)
        {
            const PolygonSoupSpacialNode* lpaLevelNodes = mapParentNodes[liLevel];

            const u16 lu16CandidatesThisLevel = lu16NumCandidates;
            lu16NumCandidates = 0;

            if (lu16CandidatesThisLevel != 0)
            {
                for (u32 luCandidate = 0; luCandidate < lu16CandidatesThisLevel; ++luCandidate)
                {
                    const PolygonSoupSpacialNode& lrNode = lpaLevelNodes[lpaPing[luCandidate]];

                    if (!BoxesOverlapXYZ(lrQueryBox, lrNode.mBox))
                    {
                        continue;
                    }

                    if (lrNode.mu16NumIndices == 0)
                    {
                        continue;
                    }

                    for (u32 luIndex = 0; luIndex < lrNode.mu16NumIndices; ++luIndex)
                    {
                        // .cpp:447 -- the console streams "level N of M" and both box corners
                        // into the message; CGS_ASSERT here takes a literal.
                        CGS_ASSERT(static_cast<s32>(lu16NumCandidates) < miQueryBufferSize,
                                   "Too many results in level ");

                        lpaPong[lu16NumCandidates] = lrNode.mpaIndices[luIndex];
                        ++lu16NumCandidates;
                    }
                }
            }

            u16* lpaSwap = lpaPing;
            lpaPing      = lpaPong;
            lpaPong      = lpaSwap;
        }

        // The buffer the last level WROTE (the swap has already happened), and its count.
        mpOutputQueryBuffer    = lpaPing;
        miLastQueryResultCount = static_cast<s32>(lu16NumCandidates);

        return static_cast<s32>(lu16NumCandidates);
    }

    // ------------------------------------------------------------------------
    // RunQuery(const Line&) @0x82843E98 (505) -- crash parity FX-GEOMETRIC, 2026-09-24. No body before;
    // BaseCollisionGenerator::CollideLineAgainstPolySoupList trapped where it calls this (0x82812D1C),
    // i.e. on every line of 20 m or more -- the place-on-track drop test is 100 m.
    // X360 export name `sub_82843E98`; the PS3 mangle @0xB64574 names it and types the parameter
    // (DWARF CgsPolygonSoupListSpatialMap.cpp:479, locals luNumSrcNodeIndices / luNumDestNodeIndices /
    // lpuSrcNodeIndices / lpuDestNodeIndices / liLevel / lLineStart / lLineEnd / lpNodes /
    // luNodeIndexEntry / lpNode / luNodeToAdd / lpTemp).
    //
    //   0x82843EB4  lwz miNumLevels (+0x5C) ; == 0 -> `li r3, 0` return, NOTHING written
    //   0x82843EDC  dest = mapQueryBuffers[1] (+0x54), src = mapQueryBuffers[0] (+0x50),
    //               v127 = line+0x00 (start), v123 = line+0x10 (end), `sth 0 -> src[0]`, count r9 = 1
    //   0x82843F10  miNumLevels <= 0 -> straight to the tail (publishes src with count 1)
    //   per level (0x82843FB0): lpNodes = mapParentNodes[level] (cursor this+8), dest count r27 = 0;
    //     src count == 0 -> straight to the swap
    //     per src entry (0x82843FE4): node = lpNodes + 48 * src[i] (`rotlwi 1 ; add ; slwi 4`) and
    //       TestLineStartEndAxisAlignedBox(start, end, node box) INLINED WHOLE -- the vrefp128 +
    //       three Newton-Raphson steps are recomputed per node, and so are its three "Line
    //       reciprocal X/Y/Z is 0" tripwires (CgsLineTests.cpp:441..443, 0x82844070/0x82844108/0x828441A0);
    //       no hit (`vcmpeqfp128. total, zero` all true) -> next entry
    //     `lhz 0x24(node)` == 0 -> next entry (checked before any capacity test)
    //     per index j (0x828443F4): `clrlwi r27,16 ; lwz 0x60 ; clrlwi 16 ; cmplw` -- the capacity test
    //       is (u16)count < (u16)miQueryBufferSize; else "Too many results in level " << level << " of "
    //       << miNumLevels << "\n LineStart: " << start << "\n LineEnd: " << end << "\n"
    //       (CgsPolygonSoupListSpatialMap.cpp:523, 0x20B, non-gating) ; dest[count++] = node indices[j]
    //   0x82844620  swap src/dest, ++level, loop while level < miNumLevels (`cmpw`, signed)
    //   0x8284465C  mpOutputQueryBuffer (+0x58) = src (the buffer the last level wrote),
    //               miLastQueryResultCount (+0x64) = (u16)count ; return (u16)count
    // No ReadOnlyObjectCache here (unlike RunJobQuery): the node arrays are indexed directly.
    // ------------------------------------------------------------------------
    s32 PolygonSoupListSpatialMap::RunQuery(const Line& lrLine)
    {
        if (miNumLevels == 0)
        {
            return 0;   // `li r3, 0`; the output fields are untouched
        }

        u16* lpuSrcNodeIndices  = mapQueryBuffers[0];   // +0x50
        u16* lpuDestNodeIndices = mapQueryBuffers[1];   // +0x54

        // v127 / v123: the segment's two 16-byte lanes, w included.
        const Vector4 lLineStart = { lrLine.mStart.x, lrLine.mStart.y, lrLine.mStart.z, lrLine.mStart.w };
        const Vector4 lLineEnd   = { lrLine.mEnd.x,   lrLine.mEnd.y,   lrLine.mEnd.z,   lrLine.mEnd.w };

        // Seed: the single root node of level 0.
        lpuSrcNodeIndices[0] = 0;
        u16 luNumSrcNodeIndices = 1;

        for (s32 liLevel = 0; liLevel < miNumLevels; ++liLevel)
        {
            const PolygonSoupSpacialNode* lpNodes = mapParentNodes[liLevel];
            u16 luNumDestNodeIndices = 0;

            for (u16 luNodeIndexEntry = 0; luNodeIndexEntry < luNumSrcNodeIndices; ++luNodeIndexEntry)
            {
                const PolygonSoupSpacialNode& lrNode = lpNodes[lpuSrcNodeIndices[luNodeIndexEntry]];

                if (!TestLineStartEndAxisAlignedBox(lLineStart, lLineEnd, lrNode.mBox))
                {
                    continue;
                }

                for (u16 luNodeToAdd = 0; luNodeToAdd < lrNode.mu16NumIndices; ++luNodeToAdd)
                {
                    // :523 -- the console streams "level N of M", the line's start and end into the
                    // message; CGS_ASSERT takes a literal. Both sides are compared as u16.
                    CGS_ASSERT(luNumDestNodeIndices < static_cast<u16>(miQueryBufferSize),
                               "Too many results in level ");

                    lpuDestNodeIndices[luNumDestNodeIndices] = lrNode.mpaIndices[luNodeToAdd];
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
        mpOutputQueryBuffer    = lpuSrcNodeIndices;
        miLastQueryResultCount = static_cast<s32>(luNumSrcNodeIndices);

        return static_cast<s32>(luNumSrcNodeIndices);
    }
}
