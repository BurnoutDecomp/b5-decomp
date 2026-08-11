#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox4.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupList.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"

// ============================================================================
// CgsGeometric::PolygonSoupListSpatialMap::BuildSpacialPartition @0x82841740
//   -- reconstructed from BURNOUT_X360_ARTIST.XEX (2,255 instructions).
//
// This is the function that turns the registered PolygonSoupLists into the
// queryable structure everything downstream needs. Until it runs, mpLeafNodes is
// null and TriangleCacheManager::StartUpdateTriangleCaches @0x828BECF8 returns at
// its own first guard (`lwz r11, 0x48(r29) ; beq epilogue`), so no triangle cache
// is ever filled.
//
// ARITY / PARAMETER NAMES are DWARF, not guessed. IDA's X360 prototype prints
// EIGHTEEN int parameters under "local variable allocation has failed" and is
// garbage. The PS3 mangle @0xB605EC settles it:
//   ._ZN12CgsGeometric25PolygonSoupListSpatialMap21BuildSpacialPartition
//     EPN9CgsMemory12LinearMallocEii
//   void BuildSpacialPartition(CgsMemory::LinearMalloc* lpAllocator,
//                              int32_t liNumLevels, int32_t liQueryBufferSize)
// ⚠️ The declaration this replaces named the last two `liListsPerNode` /
// `liItemBudget` and described them as a per-node list cap and an item budget.
// That reading was wrong: DWARF names them liNumLevels / liQueryBufferSize, the
// body's own assert is "Num levels must be between 1 and 8", and the sole call
// site passes (…, 8, 2048) -- 8 == KI_MAX_LEVELS, 2048 == miQueryBufferSize.
//
// WHAT IT BUILDS: a UNIFORM QUADTREE OVER XZ (full extent kept in Y). Level L
// holds a (1<<L) x (1<<L) grid of 48-byte PolygonSoupSpacialNode, laid out
// row-major as [liX * levelSize + liZ]; a node's four children in level L+1 are
// (2liX, 2liZ), (2liX+1, 2liZ), (2liX, 2liZ+1), (2liX+1, 2liZ+1). The BOTTOM
// level's nodes instead index the flat PolygonSoupLeafNode array -- one leaf per
// polygon soup -- and are filled by a two-pass counting sort. Finally every
// node's box is refitted bottom-up from its children.
//
// ⚠️⚠️ EVERY node/array carve below uses sizeof(), never a console byte literal.
// The nodes are CARVED AT RUNTIME (not deserialised), so their pointer slots
// widen 4->8 on x64; sizeof happens to stay 48 because Vector4's alignas(16)
// absorbs it, and that coincidence is GATED in
// CgsPolygonSoupSpacialNode_embed_check.cpp rather than trusted.
//
// FAITHFULNESS NOTE (flagged, not hidden): the console computes each grid index
// with `vrefp` + two Newton-Raphson steps followed by `fctiwz`. A single-precision
// divide is at least as accurate as that refined reciprocal, so the truncation
// agrees everywhere except possibly on an exact cell boundary, where the console
// itself is at the mercy of its estimate. Written as a plain divide, with the same
// truncating (toward-zero) conversion. This is the same faithful-not-bit-exact
// call the cache-manager wave made for vrsqrtefp.
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // Small helpers kept local to this TU: the console does all of this with
    // vminfp / vmaxfp / vspltw on whole registers.
    // ------------------------------------------------------------------------
    namespace
    {
        // vminfp / vmaxfp are full 4-lane operations on the console; the w lane is
        // carried along but never consumed.
        inline void AccumulateMin(Vector4& lrAccum, const Vector4& lrValue)
        {
            if (lrValue.x < lrAccum.x) lrAccum.x = lrValue.x;
            if (lrValue.y < lrAccum.y) lrAccum.y = lrValue.y;
            if (lrValue.z < lrAccum.z) lrAccum.z = lrValue.z;
            if (lrValue.w < lrAccum.w) lrAccum.w = lrValue.w;
        }

        inline void AccumulateMax(Vector4& lrAccum, const Vector4& lrValue)
        {
            if (lrValue.x > lrAccum.x) lrAccum.x = lrValue.x;
            if (lrValue.y > lrAccum.y) lrAccum.y = lrValue.y;
            if (lrValue.z > lrAccum.z) lrAccum.z = lrValue.z;
            if (lrValue.w > lrAccum.w) lrAccum.w = lrValue.w;
        }

        // The console seeds each refit accumulator with the node's own box CENTRE
        // (vaddfp of the two corners, times vcsxwfp128(1,1) == 0.5), all four lanes.
        inline Vector4 BoxCentre(const AxisAlignedBox& lrBox)
        {
            Vector4 lvCentre;
            lvCentre.x = (lrBox.mMin.x + lrBox.mMax.x) * 0.5f;
            lvCentre.y = (lrBox.mMin.y + lrBox.mMax.y) * 0.5f;
            lvCentre.z = (lrBox.mMin.z + lrBox.mMax.z) * 0.5f;
            lvCentre.w = (lrBox.mMin.w + lrBox.mMax.w) * 0.5f;
            return lvCentre;
        }

        inline void SetZero(Vector4& lrVector)
        {
            lrVector.x = 0.0f;   // vspltisw v0, 0 ; stvx128
            lrVector.y = 0.0f;
            lrVector.z = 0.0f;
            lrVector.w = 0.0f;
        }
    }

    void PolygonSoupListSpatialMap::BuildSpacialPartition(
        CgsMemory::LinearMalloc* lpAllocator, s32 liNumLevels, s32 liQueryBufferSize)
    {
        // -- 0. Guards + the opening banner --------------------------------------
        // :172 / :173, both strings read out of the X360 image.
        CGS_ASSERT(lpAllocator != nullptr, "No allocator specified\n");
        CGS_ASSERT(liNumLevels >= 1 && liNumLevels <= KI_MAX_LEVELS,
                   "Num levels must be between 1 and 8\n");

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Building spacial partition\n";

        // The running world bounds over every polygon soup. The console zeroes two
        // stack vectors here and folds every leaf box into them, so an EMPTY map
        // legitimately keeps the degenerate {0,0,0}..{0,0,0} box -- reproduced.
        Vector4 lvGlobalMin;
        Vector4 lvGlobalMax;
        SetZero(lvGlobalMin);
        SetZero(lvGlobalMax);

        lpAllocator->SetAlignment(16);

        // -- 1. How many leaves will there be? -----------------------------------
        // One leaf per polygon soup, summed over every registered list.
        s32 liTotalPolygonSoups = 0;
        for (s32 liList = 0; liList < miNumSoupLists; ++liList)
            liTotalPolygonSoups += GetPolySoupList(liList)->miNumPolySoups;

        // -- 2. One parent-node grid per level -----------------------------------
        // Level L has (1<<L)^2 == 4^L nodes; the console tracks that with `slwi r28, r28, 2`.
        {
            s32 liLevelNodes = 1;
            for (s32 liLevel = 0; liLevel < liNumLevels; ++liLevel)
            {
                mapParentNodes[liLevel] = static_cast<PolygonSoupSpacialNode*>(
                    lpAllocator->Malloc(sizeof(PolygonSoupSpacialNode) *
                                        static_cast<size_t>(liLevelNodes)));
                maiParentNodeCounts[liLevel] = liLevelNodes;

                // :206 "Failed to allocate parent node array "
                CGS_ASSERT(mapParentNodes[liLevel] != nullptr,
                           "Failed to allocate parent node array ");

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Allocated parent node level " << liLevel
                        << ", Used: "  << static_cast<u32>(lpAllocator->GetUsage())
                        << ", Free: "  << static_cast<u32>(lpAllocator->GetFreeMemory())
                        << "\n";
                }

                liLevelNodes <<= 2;
            }
        }

        // -- 3. The flat leaf array ----------------------------------------------
        PolygonSoupLeafNode* lpaLeafNodes = static_cast<PolygonSoupLeafNode*>(
            lpAllocator->Malloc(sizeof(PolygonSoupLeafNode) *
                                static_cast<size_t>(liTotalPolygonSoups)));
        miLeafNodeCount = liTotalPolygonSoups;

        // :213 "Failed to allocate " <n> " leaf nodes\n"
        CGS_ASSERT(lpaLeafNodes != nullptr, "Failed to allocate  leaf nodes\n");

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint
                << "Allocated " << liTotalPolygonSoups
                << " leaf nodes, Used: " << static_cast<u32>(lpAllocator->GetUsage())
                << ", Free: "            << static_cast<u32>(lpAllocator->GetFreeMemory())
                << "\n";
        }

        // -- 4. Fill the leaves, and learn the world bounds while doing it -------
        s32 liLeaf = 0;
        for (s32 liList = 0; liList < miNumSoupLists; ++liList)
        {
            const PolygonSoupList* lpList = GetPolySoupList(liList);

            // The soup pointer table and the box array, both relocated by FixUp.
            // ⚠️ The table entry width is the HOST pointer width (8 on x64): the PC
            // porter emits a u64 table ('<%dQ'), which is why this indexes as
            // PolygonSoup* const* and not by the console's 4-byte step.
            const PolygonSoup* const* lpapSoups =
                reinterpret_cast<const PolygonSoup* const*>(lpList->mpapPolySoups);
            const AxisAlignedBox4* lpaBoxes =
                reinterpret_cast<const AxisAlignedBox4*>(lpList->mpaPolySoupBoxes);

            for (s32 liSoup = 0; liSoup < lpList->miNumPolySoups; ++liSoup)
            {
                // The boxes are packed four to an AxisAlignedBox4 block.
                const s32 li4BoxIndex = liSoup / 4;
                const s32 liBoxLane   = liSoup % 4;

                // :196, from CgsPolygonSoupList.h -- an INLINED accessor assert. The
                // console's bound is (miNumPolySoups + 3) >> 2, i.e. the block count.
                CGS_ASSERT(li4BoxIndex >= 0 && li4BoxIndex < ((lpList->miNumPolySoups + 3) >> 2),
                           "li4BoxIndex >= 0 && li4BoxIndex < GetNum4Boxes()");

                PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[liLeaf];
                lrLeaf.mBox = lpaBoxes[li4BoxIndex].GetAxisAlignedBox(
                    static_cast<u32>(liBoxLane));

                // :167, likewise an inlined CgsPolygonSoupList.h accessor assert.
                CGS_ASSERT(liSoup >= 0 && liSoup < lpList->miNumPolySoups,
                           "liPolySoupIndex >= 0 && liPolySoupIndex < GetNumPolySoups()");

                lrLeaf.mpPolygonSoup = lpapSoups[liSoup];
                // The soup's own serialised byte size (CgsPolygonSoup.h mu16SoupSize,
                // X360 +0x18, x64 +0x20 -- reached BY NAME so the widening is automatic).
                // The leaf carries it so the tester job knows how much to fetch.
                lrLeaf.mu16PolygonSoupSize = lrLeaf.mpPolygonSoup->mu16SoupSize;

                AccumulateMin(lvGlobalMin, lrLeaf.mBox.mMin);
                AccumulateMax(lvGlobalMax, lrLeaf.mBox.mMax);

                ++liLeaf;
            }
        }

        // -- 5. Lay out every level's grid, and wire the child indices -----------
        {
            s32 liLevelSize = 1;   // nodes per axis at this level
            for (s32 liLevel = 0; liLevel < liNumLevels; ++liLevel)
            {
                // The cell spans the world bounds on X and Z; Y keeps the FULL extent,
                // so this is a 2D partition of a 3D world (the console only divides
                // lanes 0 and 2 -- lane 1 goes through the vperm untouched).
                const f32 lfInvLevelSize = 1.0f / static_cast<f32>(liLevelSize);
                Vector4 lvCell;
                lvCell.x = (lvGlobalMax.x - lvGlobalMin.x) * lfInvLevelSize;
                lvCell.y = (lvGlobalMax.y - lvGlobalMin.y);
                lvCell.z = (lvGlobalMax.z - lvGlobalMin.z) * lfInvLevelSize;
                lvCell.w = lvCell.x;   // the vperm's spare lane

                // The next finer level, which the child indices address.
                const s32 liChildLevelSize  = liLevelSize * 2;
                const s32 liChildNodeCount  = liChildLevelSize * liChildLevelSize;

                PolygonSoupSpacialNode* lpaNodes = mapParentNodes[liLevel];
                s32 liNode = 0;

                for (s32 liX = 0; liX < liLevelSize; ++liX)
                {
                    for (s32 liZ = 0; liZ < liLevelSize; ++liZ, ++liNode)
                    {
                        PolygonSoupSpacialNode& lrNode = lpaNodes[liNode];

                        lrNode.mBox.mMin.x =
                            lvGlobalMin.x + lvCell.x * static_cast<f32>(liX);
                        lrNode.mBox.mMin.y = lvGlobalMin.y;
                        lrNode.mBox.mMin.z =
                            lvGlobalMin.z + lvCell.z * static_cast<f32>(liZ);
                        lrNode.mBox.mMin.w = lrNode.mBox.mMin.x;

                        // vaddfp128 v0, v127, v126 -- max = min + cell, all four lanes.
                        lrNode.mBox.mMax.x = lrNode.mBox.mMin.x + lvCell.x;
                        lrNode.mBox.mMax.y = lrNode.mBox.mMin.y + lvCell.y;
                        lrNode.mBox.mMax.z = lrNode.mBox.mMin.z + lvCell.z;
                        lrNode.mBox.mMax.w = lrNode.mBox.mMin.w + lvCell.w;

                        // Four child indices, carved one 8-byte block per node.
                        lrNode.mpaIndices = static_cast<u16*>(
                            lpAllocator->Malloc(4u * sizeof(u16)));
                        lrNode.mu16NumIndices = 4;

                        // Row-major [liX * levelSize + liZ] at the finer level:
                        //   (2liX, 2liZ) (2liX+1, 2liZ) (2liX, 2liZ+1) (2liX+1, 2liZ+1)
                        const s32 liChild0 = (2 * liX)     * liChildLevelSize + (2 * liZ);
                        const s32 liChild1 = (2 * liX + 1) * liChildLevelSize + (2 * liZ);

                        lrNode.mpaIndices[0] = static_cast<u16>(liChild0);
                        lrNode.mpaIndices[1] = static_cast<u16>(liChild1);
                        lrNode.mpaIndices[2] = static_cast<u16>(liChild0 + 1);
                        lrNode.mpaIndices[3] = static_cast<u16>(liChild1 + 1);

                        // :268 .. :271 -- one per child. The console re-reads each index
                        // back out of the freshly-stored array (lhz) before comparing, so
                        // these are genuinely u16 comparisons.
                        CGS_ASSERT(lrNode.mpaIndices[0] < liChildNodeCount,
                                   "Index 0 is out of range: Index=");
                        CGS_ASSERT(lrNode.mpaIndices[1] < liChildNodeCount,
                                   "Index 1 is out of range: Index=");
                        CGS_ASSERT(lrNode.mpaIndices[2] < liChildNodeCount,
                                   "Index 2 is out of range: Index=");
                        CGS_ASSERT(lrNode.mpaIndices[3] < liChildNodeCount,
                                   "Index 3 is out of range: Index=");
                    }
                }

                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Allocated indices for level " << liLevel
                        << ", Used: " << static_cast<u32>(lpAllocator->GetUsage())
                        << ", Free: " << static_cast<u32>(lpAllocator->GetFreeMemory())
                        << "\n";
                }

                liLevelSize <<= 1;
            }
        }

        // -- 6. Bucket every leaf into the BOTTOM level, counting-sort style -----
        const s32 liLevelWidth = 1 << (liNumLevels - 1);
        const s32 liNodeCount  = liLevelWidth * liLevelWidth;
        PolygonSoupSpacialNode* lpaBottom = mapParentNodes[liNumLevels - 1];

        // The bottom level's cell, recomputed exactly as the console does (it does not
        // reuse the loop's last value; it re-derives from liNumLevels - 1).
        Vector4 lvBottomCell;
        {
            const f32 lfInvWidth = 1.0f / static_cast<f32>(liLevelWidth);
            lvBottomCell.x = (lvGlobalMax.x - lvGlobalMin.x) * lfInvWidth;
            lvBottomCell.y = (lvGlobalMax.y - lvGlobalMin.y);
            lvBottomCell.z = (lvGlobalMax.z - lvGlobalMin.z) * lfInvWidth;
            lvBottomCell.w = lvBottomCell.x;
        }

        // PASS 1 -- count. :296 / :297 / :298.
        for (s32 li = 0; li < miLeafNodeCount; ++li)
        {
            const PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[li];

            // centre = (min + max) * 0.5  (vaddfp then vmulfp by vcsxwfp128(1,1))
            const f32 lfCentreX = (lrLeaf.mBox.mMin.x + lrLeaf.mBox.mMax.x) * 0.5f;
            const f32 lfCentreZ = (lrLeaf.mBox.mMin.z + lrLeaf.mBox.mMax.z) * 0.5f;

            const s32 liX = static_cast<s32>((lfCentreX - lvGlobalMin.x) / lvBottomCell.x);
            const s32 liZ = static_cast<s32>((lfCentreZ - lvGlobalMin.z) / lvBottomCell.z);
            const s32 liNode = liX * liLevelWidth + liZ;

            CGS_ASSERT(liX >= 0 && liX < liLevelWidth, "X is out of range: liX=");
            CGS_ASSERT(liZ >= 0 && liZ < liLevelWidth, "Z is out of range: liZ=");
            CGS_ASSERT(liNode >= 0 && liNode < liNodeCount, "Node Index is out of range: liNode=");

            ++lpaBottom[liNode].mu16NumIndices;
        }

        // ALLOCATE -- one index array per non-empty bottom node, then rewind the
        // count so pass 2 can use it as the write cursor. ⚠️ A node with no leaves
        // keeps the mpaIndices its level-5 pass gave it (the 4-entry child block) and
        // a count of zero; the console does exactly this, so the stale pointer is
        // as-shipped, not an oversight of mine.
        for (s32 liNode = 0; liNode < liNodeCount; ++liNode)
        {
            PolygonSoupSpacialNode& lrNode = lpaBottom[liNode];
            if (lrNode.mu16NumIndices == 0)
                continue;

            lrNode.mpaIndices = static_cast<u16*>(
                lpAllocator->Malloc(static_cast<size_t>(lrNode.mu16NumIndices) * sizeof(u16)));

            // :311
            CGS_ASSERT(lrNode.mpaIndices != nullptr,
                       "Failed to allocate indices for bottom level");

            lrNode.mu16NumIndices = 0;
        }

        // PASS 2 -- fill. Identical index math; :323 / :324 / :325.
        for (s32 li = 0; li < miLeafNodeCount; ++li)
        {
            const PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[li];

            const f32 lfCentreX = (lrLeaf.mBox.mMin.x + lrLeaf.mBox.mMax.x) * 0.5f;
            const f32 lfCentreZ = (lrLeaf.mBox.mMin.z + lrLeaf.mBox.mMax.z) * 0.5f;

            const s32 liX = static_cast<s32>((lfCentreX - lvGlobalMin.x) / lvBottomCell.x);
            const s32 liZ = static_cast<s32>((lfCentreZ - lvGlobalMin.z) / lvBottomCell.z);
            const s32 liNode = liX * liLevelWidth + liZ;

            CGS_ASSERT(liX >= 0 && liX < liLevelWidth, "X is out of range: liX=");
            CGS_ASSERT(liZ >= 0 && liZ < liLevelWidth, "Z is out of range: liZ=");
            CGS_ASSERT(liNode >= 0 && liNode < liNodeCount, "Node Index is out of range: liNode=");

            PolygonSoupSpacialNode& lrNode = lpaBottom[liNode];
            lrNode.mpaIndices[lrNode.mu16NumIndices] = static_cast<u16>(li);
            ++lrNode.mu16NumIndices;
        }

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint
                << "Allocated bottom level indices, Used: " << static_cast<u32>(lpAllocator->GetUsage())
                << ", Free: " << static_cast<u32>(lpAllocator->GetFreeMemory())
                << "\n";
        }

        // -- 7. Refit every node's box, bottom-up --------------------------------
        // The console writes this twice because the element types differ: the bottom
        // level unions LEAF boxes, every level above it unions child NODE boxes.
        // ⭐ Both seed the accumulator with the node's OWN CENTRE, so a childless node
        // collapses to a point rather than to an inverted/infinite box.
        {
            for (s32 liNode = 0; liNode < liNodeCount; ++liNode)
            {
                PolygonSoupSpacialNode& lrNode = lpaBottom[liNode];

                Vector4 lvMin = BoxCentre(lrNode.mBox);
                Vector4 lvMax = lvMin;

                for (u16 lu16 = 0; lu16 < lrNode.mu16NumIndices; ++lu16)
                {
                    const PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lrNode.mpaIndices[lu16]];
                    AccumulateMin(lvMin, lrLeaf.mBox.mMin);
                    AccumulateMax(lvMax, lrLeaf.mBox.mMax);
                }

                lrNode.mBox.mMin = lvMin;
                lrNode.mBox.mMax = lvMax;
            }

            for (s32 liLevel = liNumLevels - 2; liLevel >= 0; --liLevel)
            {
                PolygonSoupSpacialNode*       lpaLevel = mapParentNodes[liLevel];
                const PolygonSoupSpacialNode* lpaChild = mapParentNodes[liLevel + 1];
                const s32 liLevelNodes = 1 << (2 * liLevel);   // slw r30, 1, 2*liLevel

                for (s32 liNode = 0; liNode < liLevelNodes; ++liNode)
                {
                    PolygonSoupSpacialNode& lrNode = lpaLevel[liNode];

                    Vector4 lvMin = BoxCentre(lrNode.mBox);
                    Vector4 lvMax = lvMin;

                    for (u16 lu16 = 0; lu16 < lrNode.mu16NumIndices; ++lu16)
                    {
                        const PolygonSoupSpacialNode& lrChild = lpaChild[lrNode.mpaIndices[lu16]];
                        AccumulateMin(lvMin, lrChild.mBox.mMin);
                        AccumulateMax(lvMax, lrChild.mBox.mMax);
                    }

                    lrNode.mBox.mMin = lvMin;
                    lrNode.mBox.mMax = lvMax;
                }
            }
        }

        // -- 8. The query scratch, and publish the partition ---------------------
        mapQueryBuffers[0] = static_cast<u16*>(
            lpAllocator->Malloc(static_cast<size_t>(liQueryBufferSize) * sizeof(u16)));
        CGS_ASSERT(mapQueryBuffers[0] != nullptr, "Failed to allocate query buffer 0\n");   // :382

        mapQueryBuffers[1] = static_cast<u16*>(
            lpAllocator->Malloc(static_cast<size_t>(liQueryBufferSize) * sizeof(u16)));
        CGS_ASSERT(mapQueryBuffers[1] != nullptr, "Failed to allocate query buffer 1\n");   // :384

        // ⚠️ FLAG, as-shipped: mpOutputQueryBuffer (+0x58) is NOT written here. There
        // are exactly six Mallocs in the whole function and none is for it, so after a
        // Clear()/BuildSpacialPartition() cycle it stays null. Reproduced faithfully;
        // whoever lands RunQuery @0x82843A80 needs to know this.
        miNumLevels       = liNumLevels;        // +0x5C
        mpLeafNodes       = lpaLeafNodes;       // +0x48 -- the "is there a partition?" flag
        miQueryBufferSize = liQueryBufferSize;  // +0x60

        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint
                << "Spacial map complete, Used: " << static_cast<u32>(lpAllocator->GetUsage())
                << ", Free: " << static_cast<u32>(lpAllocator->GetFreeMemory())
                << "\n";
        }
    }
}
