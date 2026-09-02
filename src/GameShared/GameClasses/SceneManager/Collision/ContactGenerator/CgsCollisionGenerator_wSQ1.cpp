// =================================================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_wSQ1.cpp
//
// BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0 (494 insns) --
// THE RAY-vs-STATIC-WORLD KERNEL DRIVER of the scene-query pipeline. Scene-query wave 1 landed it
// as a loud trap (2026-09-02, morning); wave 1b (same day) bodies the short-line arm on top of
// PolygonSoupListSpatialMap::RunQuery @0x82843A80 and CgsGeometric::
// IntersectLinePolygonSoupNearestSingleSided @0x8283BC98, both reconstructed in the same commit.
//
// Reachability: SceneManagerModule::ProcessTriangleCollisionLineTestNearests (direct arm) and
// ProcessLineTestNearest (the world-race arm) -- every race car's above-ground ray, every frame.
//
// ---- DECODE, address by address ----------------------------------------------------------------
//   0x828131E4..0x82813244  CGS_ASSERT(Line::IsValid(line), "Invalid line") -- :1023
//   0x8281324C  bl PrepareNewPrimitiveTestResultsList(1, tagA, tagB) -> r3 = the index (returned)
//   0x82813254  lvx128 v127 <- line+0x00 (start, all four lanes); v123 <- line+0x10 (end)
//   0x82813260  vsubfp128 v126 = end - start
//   0x82813268  `vspltisw v0, 2 ; vcfsx v0, v0, 0` = {2,2,2,2}
//   0x82813278  r16 = 4*(idx + 0x4820) -> mapCollisionResultLists[idx] (0x12080 == the console
//               offsetof; reached BY NAME here) ; r15 = list->mpResults ; r14 = r15 + 0x50
//   0x82813290  stvx128 v0 -> [r14]  : the single CollisionResult's +0x50 lane seeded to 2.0
//   0x82813298  vmsum3fp128 v12 = |end-start|^2 (xyz) ; vcmpgtfp. 400.0 > v12
//   -- short arm (under 20 m) --
//   0x828132B4  vmaxfp/vminfp of start,end -> AxisAlignedBox {min,max} on the stack
//   0x828132C8  bl PolygonSoupListSpatialMap::RunQuery(map, &box) -> r3 = leaf count
//   loop        idx = map->mpOutputQueryBuffer[i] (+0x58) ; leaf = map->mpLeafNodes (+0x48) +
//               48*idx ; six vcmpgefp lanes (leaf.max >= box.min, box.max >= leaf.min), xyz
//               reduced by vpermwi 0x4B/0x87 ; skip on no overlap ;
//               bl IntersectLinePolygonSoupNearestSingleSided(leaf.mpPolygonSoup (+0x20), tmp)
//               with v1 = start, v2 = end ; `vcmpeqfp128. v0, v1, zero` -> skip on no hit ;
//               `vcmpgtfp. best+0x50, tmp+0x50` -> copy 14 qwords tmp -> best when strictly nearer
//   -- long arm (20 m and over) --
//   0x82813444  bl sub_82843E98(map, &line) -- the line-slab leaf gather -- then per leaf a
//               reciprocal-direction slab clip (the three "Line reciprocal X/Y/Z is 0" tripwires,
//               CgsLineTests.cpp:441/442/443) before the same soup test. NOT RECONSTRUCTED: a
//               loud trap below (no caller in the tree asks for a line that long -- the race car's
//               above-ground ray is 10 m).
//   0x82813930  `vcfsx 1.0 ; vcmpgefp. 1.0 >= best+0x50` ; `sth -> list+0x0C` (mu16NumResults)
//               = 1 on a hit, 0 otherwise. This is the ONLY write of the count on this leg.
//   return      the result-list index.
//
// ⚠️ CONSOLE QUIRK, REPRODUCED, NOT FIXED: PrepareNewPrimitiveTestResultsList(1, ...) allocates
// KI_PRIMITIVE_TEST_RESULT_MEMORY_SIZE * 1 == 80 bytes for the result, but the record this leg
// keeps in it is 112 bytes (+0x50 t lane, +0x60 tag lane -- exactly the bytes past the block). The
// console writes them into the linear allocator's not-yet-claimed tail; both consumers read the
// record BEFORE the next Prepare* claims anything, so it works by ordering. Same here (the
// allocator is CgsMemory::LinearMalloc, a bump allocator with slack). Widening the allocation
// would be an invented arm and would move every later block.
// =================================================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"            // AxisAlignedBox
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                      // CgsGeometric::Line
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h" // PolygonSoupListSpatialMap
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h" // PolygonSoupLeafNode
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"       // IntersectLinePolygonSoupNearestSingleSided
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h" // CollisionResultList

#include <cstring>   // std::memcpy (the 14-qword result copy)

namespace CgsSceneManager
{
namespace CgsCollision
{
    namespace
    {
        // 0x82813298: `vmsum3fp128` vs the 400.0 splat (flt at 0x828131C0's literal pool; 20 m^2).
        const f32 KF_SHORT_LINE_LENGTH_SQ = 400.0f;

        // 0x82813268: the "no hit yet" line parameter seed.
        const f32 KF_LINE_PARAM_NO_HIT = 2.0f;

        // The six-lane box overlap the loop does inline (leaf.max >= box.min && box.max >= leaf.min,
        // xyz only -- `vpermwi128 0x4B / 0x87 ; vspltw 0` drop the w lane).
        inline bool LeafOverlapsBoxXYZ(const CgsGeometric::AxisAlignedBox& lrLeaf,
                                       const CgsGeometric::AxisAlignedBox& lrBox)
        {
            if (!(lrLeaf.mMax.x >= lrBox.mMin.x)) return false;
            if (!(lrLeaf.mMax.y >= lrBox.mMin.y)) return false;
            if (!(lrLeaf.mMax.z >= lrBox.mMin.z)) return false;
            if (!(lrBox.mMax.x >= lrLeaf.mMin.x)) return false;
            if (!(lrBox.mMax.y >= lrLeaf.mMin.y)) return false;
            if (!(lrBox.mMax.z >= lrLeaf.mMin.z)) return false;
            return true;
        }
    }

    // @ 0x828131C0
    u16 BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest(
        const CgsGeometric::Line&                 lrLine,
        CgsGeometric::PolygonSoupListSpatialMap*  lpPolySoupListSpacialMap,
        u32                                       lu32UserTagA,
        u16                                       lu16UserTagB)
    {
        CGS_ASSERT(lrLine.IsValid(), "Invalid line\n");   // :1023

        const s32 liResultListIndex = PrepareNewPrimitiveTestResultsList(1, lu32UserTagA, lu16UserTagB);

        // 0x82813278..0x82813290: the list's single record, its +0x50 lane seeded to 2.0. The rest
        // of the record is whatever the allocator held until the first strictly-nearer copy.
        CollisionResultList* lpList = mapCollisionResultLists[static_cast<u16>(liResultListIndex)];
        CgsGeometric::PolySoupLineNearestResult* lpBest =
            reinterpret_cast<CgsGeometric::PolySoupLineNearestResult*>(lpList->mpResults);
        lpBest->mLineParam.x = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.y = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.z = KF_LINE_PARAM_NO_HIT;
        lpBest->mLineParam.w = KF_LINE_PARAM_NO_HIT;

        // v127 / v123: the two 16-byte lanes of the line, w included (the soup test's +0x40
        // position lane multiplies all four).
        const Vector3& lrStart = reinterpret_cast<const Vector3&>(lrLine.mStart);
        const Vector3& lrEnd   = reinterpret_cast<const Vector3&>(lrLine.mEnd);

        const f32 lfDx = lrEnd.x - lrStart.x;
        const f32 lfDy = lrEnd.y - lrStart.y;
        const f32 lfDz = lrEnd.z - lrStart.z;
        const f32 lfLengthSq = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        if (KF_SHORT_LINE_LENGTH_SQ > lfLengthSq)
        {
            // 0x828132B4..0x828132C4: the line's box, all four lanes min/max'd.
            CgsGeometric::AxisAlignedBox lBox;
            lBox.mMin.x = (lrStart.x < lrEnd.x) ? lrStart.x : lrEnd.x;
            lBox.mMin.y = (lrStart.y < lrEnd.y) ? lrStart.y : lrEnd.y;
            lBox.mMin.z = (lrStart.z < lrEnd.z) ? lrStart.z : lrEnd.z;
            lBox.mMin.w = (lrStart.w < lrEnd.w) ? lrStart.w : lrEnd.w;
            lBox.mMax.x = (lrStart.x > lrEnd.x) ? lrStart.x : lrEnd.x;
            lBox.mMax.y = (lrStart.y > lrEnd.y) ? lrStart.y : lrEnd.y;
            lBox.mMax.z = (lrStart.z > lrEnd.z) ? lrStart.z : lrEnd.z;
            lBox.mMax.w = (lrStart.w > lrEnd.w) ? lrStart.w : lrEnd.w;

            const s32 liNumLeaves = lpPolySoupListSpacialMap->RunQuery(lBox);   // 0x828132C8

            if (liNumLeaves > 0)
            {
                const u16*                             lpau16Leaves = lpPolySoupListSpacialMap->GetOutputQueryBuffer();
                const CgsGeometric::PolygonSoupLeafNode* lpaLeafNodes = lpPolySoupListSpacialMap->GetLeafNodes();

                CgsGeometric::PolySoupLineNearestResult lTemp;   // sp+0x120, 112 bytes

                for (s32 liLeaf = 0; liLeaf < liNumLeaves; ++liLeaf)
                {
                    const CgsGeometric::PolygonSoupLeafNode& lrLeaf = lpaLeafNodes[lpau16Leaves[liLeaf]];

                    if (!LeafOverlapsBoxXYZ(lrLeaf.mBox, lBox))
                    {
                        continue;
                    }

                    if (!CgsGeometric::IntersectLinePolygonSoupNearestSingleSided(*lrLeaf.mpPolygonSoup, &lTemp,
                                                                                  lrStart, lrEnd))
                    {
                        continue;   // `vcmpeqfp128. v0, v1, zero` -- no hit in this soup
                    }

                    // 0x828133E8..0x828133F4: `vcmpgtfp. best+0x50, tmp+0x50` -- STRICTLY nearer
                    // replaces (all four lanes carry the same splatted value).
                    if (lpBest->mLineParam.x > lTemp.mLineParam.x)
                    {
                        std::memcpy(lpBest, &lTemp, sizeof(CgsGeometric::PolySoupLineNearestResult));   // 14 qwords
                    }
                }
            }
        }
        else
        {
            // 0x82813440 sub_82843E98(map, &line) + the slab-clipped per-leaf test. A line of 20 m
            // or more has no producer in the tree today; when one appears this fires, by name.
            CGS_ASSERT(false, "BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0: the "
                              "long-line arm (sub_82843E98 line-slab leaf gather + CgsLineTests.cpp:441 slab "
                              "clip) is not reconstructed -- a line of 20 m or more was asked for");
        }

        // 0x82813930..0x82813960: mu16NumResults = (1.0 >= best t). The one write of the count.
        lpList->mu16NumResults = (1.0f >= lpBest->mLineParam.x) ? 1 : 0;

        return static_cast<u16>(liResultListIndex);
    }
}
}
