#pragma once

// CgsSceneManager::CgsCollision::CollisionJobDescription — the shared base of every
// collision contact-generator job descriptor (sphere/triangle/primitive list testers).
//
// DWARF home path: CgsCollisionJobDescription.h (the file the per-job headers include,
// see CgsCollisionJobDescription.h:25/28). Each concrete descriptor derives from this
// base and adds its own typed `Data` payload (a pair/sphere/triangle list set).
//
// Bookkeeping fields recovered store-for-store from the X360 Prepare() asm across the
// four descriptor jobs (PrimitivePairList @0x82810478, SphereListWithSphereList
// @0x82810198, SphereListWithTriangleList @0x82810100, PrimitiveListWithTriangleList
// @0x82810278). Each Prepare() writes the SAME four bookkeeping fields:
//
//   stw   r6,  0xF0(r3)   ->  mpResultsList  (the job's results list)
//   stfs  f0,  0xF4(r3)   ->  mfRadius       (radius/padding; 0.0 for list-vs-list jobs)
//   stw   r26, 0xF8(r3)   ->  miStatus       (status/result count, init 0)
//   stb   r10, 0xFF(r3)   ->  muJobType      (job-type id: 5/7/10/11 per concrete job)
//
// SEMANTIC parity (member-by-name), not byte-match: the four bookkeeping fields sit at
// X360 offsets 0xF0..0xFF (after the derived 240-byte Data payload). Modelled here as
// named base members; the derived data lists are named members of each derived type.
// (Host pointer width differs from the X360's 4-byte pointers, so we do NOT pin the
// X360 256-byte sizeof — the recovered behaviour is the field writes, which we keep.)

#include "types.hpp"

namespace CgsSceneManager
{
namespace CgsCollision
{
    // Forward decl — the results list a tester job fills (homed in
    // Collision/Primitives/CgsCollisionResult.h).
    struct CollisionResultList;

    // Job-type ids written to +0xFF by each concrete descriptor's Prepare().
    // (li rN,<id> ; stb rN,0xFF(r3))
    enum E_CollisionJobType
    {
        // ⭐ 2026-08-10 (fill-worker wave 2): the three PolygonSoupTester ids, read off the
        // switch in PolygonSoupTesterJob::Execute @0x82915930 (case 2/3/4 dispatching to
        // ExecuteFillTriangleCache / ExecuteFillTriangleCacheStream / ExecuteLineTest) and,
        // for 3, corroborated by the writer: RunFillTriangleCacheStream @0x82810DD0
        // `li r21, 3 ; stb r21, 0x4CF(batch)`.
        E_COLLISIONJOB_FILL_TRIANGLE_CACHE                = 2,   // -> ExecuteFillTriangleCache
        E_COLLISIONJOB_FILL_TRIANGLE_CACHE_STREAM         = 3,   // -> ExecuteFillTriangleCacheStream
        E_COLLISIONJOB_LINE_WITH_POLYSOUP_STREAM          = 4,   // -> ExecuteLineTest

        E_COLLISIONJOB_SPHERE_LIST_WITH_TRIANGLE_LIST     = 5,   // @0x82810100
        E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST       = 7,   // @0x82810198
        E_COLLISIONJOB_PRIMITIVE_PAIR_LIST                = 10,  // @0x82810478
        E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST  = 11,  // @0x82810278

        // ⭐ 2026-08-11 (traction-line wave). 16 is written by the dispatcher
        // (BaseCollisionGenerator::RunLineWithTriangleListStream @0x82810F0C `li r21, 16` ->
        // `stb r21, 0x4CF(batch)`) and read straight back by ContactGeneratorJob::Execute
        // @0x82926818 (`lbz r11, 0xFF(r4)`; `addi r11, r11, -5`; 12-way `bctr`, index 11).
        // ⚠️ NAME MISMATCH CARRIED FROM THE CONSOLE: the PS3 DWARF enum member is spelled
        // `E_COLLISION_TYPE_LINE_TRIANGLE_LIST` -- it DROPS "STREAM" -- while the descriptor
        // class and the worker both keep it. The class name wins here; the console's own
        // spelling is recorded in this comment so a future name-join can find either.
        E_COLLISIONJOB_LINE_WITH_TRIANGLE_LIST_STREAM     = 16,  // @0x82810E80 -> the traction line
    };

    struct CollisionJobDescription
    {
        CollisionResultList* mpResultsList;  // X360 +0xF0  results list the job writes
        f32                  mfRadius;       // X360 +0xF4  contact radius / padding (0.0)
        s32                  miStatus;       // X360 +0xF8  status / result count (init 0)
        u8                   muJobType;      // X360 +0xFF  E_CollisionJobType id

        // X360 0x82916EB0: `lwz r3, 0xF0(r3)` — return the job's results list.
        // Called by PolygonSoupTesterJob::ExecuteFillTriangleCache.
        CollisionResultList* GetResultsList() const { return mpResultsList; }

        // GetType @0x82916E98 (6): `lbz r11, 0xFF(r11)` — the job-type id the tester's
        // Execute switches on. ⭐ ADDED 2026-08-10 (fill-worker wave 2): this is the
        // accessor PolygonSoupTesterJob::Execute calls, and it is what makes the
        // descriptor slot's identity readable back out of the batch.
        u8 GetType() const { return muJobType; }
    };
}
}
