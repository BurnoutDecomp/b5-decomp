// =================================================================================================
// GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator_wSQ1.cpp
//
// BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0 (494 insns) --
// THE RAY-vs-STATIC-WORLD KERNEL of the scene-query pipeline (scene-query wave 1, 2026-09-02).
//
// ⛔ LOUD TRAP, NOT A BODY. What the console does (read from the asm; the numbers are its own):
//   1. assert CgsGeometric::Line::IsValid(line)   ("Invalid line", CgsCollisionGenerator.cpp:1023)
//   2. idx = PrepareNewPrimitiveTestResultsList(1, tagA, tagB); seed the list's single
//      CollisionResult's +0x50 lane to {2,2,2,2} (`vspltisw 2 ; vcfsx` -- a line parameter of 2.0
//      means "no hit yet"; every real hit is in [0,1]).
//   3. if |end - start|^2 < 400 (the 20 m short-line arm, `vmsum3fp128` vs a 400.0 splat):
//        RunQuery(map, AABB(min(start,end), max(start,end)))  -- PolygonSoupListSpatialMap::RunQuery
//        @0x82843A80, 261 insns, the ping-pong twin of the RunJobQuery already in the tree;
//        for each returned leaf index: re-test the leaf's box against the line's box (six
//        vcmpgefp lanes, xyz only), then
//        IntersectLinePolygonSoupNearestSingleSided(leaf.mpPolygonSoup, tmp /*112-byte result*/)
//        @0x8283BC98 (575 insns: UnpackPolygonSoupVertices + IntersectLinePolySoupTriangleSingleSided4
//        per quad/triangle block) with the line in v1/v2; if it hit (v1 != 0) and tmp's +0x50 lane
//        is below the current best's, copy the 112 bytes over the best.
//      else (the long-line arm): sub_82843E98(map, line) -- a slab-walk leaf gather -- then the
//        same per-leaf test behind a reciprocal-direction slab clip (the three
//        "Line reciprocal X/Y/Z is 0" tripwires, CgsLineTests.cpp:441/442/443).
//   4. return idx.
//
// Nothing below the kernel is in the tree yet: RunQuery, IntersectLinePolygonSoupNearestSingleSided,
// IntersectLinePolySoupTriangleSingleSided4, sub_82843E98. A body that skipped them would return a
// result list whose only record still carries the 2.0 seed -- and the consumer would publish that as
// a MISS every frame, which is precisely the plausible-zero this project keeps getting burned by.
// So the trap fires the first time a world line test is asked for. It carries the console address.
//
// Reachability: SceneManagerModule::ProcessTriangleCollisionLineTestNearests (direct arm) and
// ProcessLineTestNearest (the world-race arm) -- i.e. every race car's above-ground ray, every frame,
// once WorldModule::BridgePhysicsSceneQueriesToScene is live. THIS IS THE NEXT WAVE'S FIRST TASK.
// =================================================================================================

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/CgsCollisionGenerator.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"                      // CgsGeometric::Line
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h" // PolygonSoupListSpatialMap

namespace CgsSceneManager
{
namespace CgsCollision
{
    // @ 0x828131C0
    u16 BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest(
        const CgsGeometric::Line&                       lrLine,
        const CgsGeometric::PolygonSoupListSpatialMap*  /*lpPolySoupListSpacialMap*/,
        u32                                             lu32UserTagA,
        u16                                             lu16UserTagB)
    {
        // :1023 -- the console's own entry tripwire, kept so an invalid line is reported as such
        // rather than as "kernel missing".
        CGS_ASSERT(lrLine.IsValid(), "Invalid line\n");

        CGS_ASSERT(false, "BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest @0x828131C0 is not "
                          "reconstructed (needs PolygonSoupListSpatialMap::RunQuery @0x82843A80 + "
                          "CgsGeometric::IntersectLinePolygonSoupNearestSingleSided @0x8283BC98)");

        // If execution continues past the trap (asserts compiled out), hand back an EMPTY list
        // -- zero results, never a record carrying a fabricated hit -- so the caller's
        // `mu16NumResults != 0` test reads as a MISS, exactly as an untouched console list with
        // its 2.0 seed would be published (the consumer keys on mbIntersection).
        return static_cast<u16>(PrepareNewPrimitiveTestResultsList(1, lu32UserTagA, lu16UserTagB));
    }
}
}
