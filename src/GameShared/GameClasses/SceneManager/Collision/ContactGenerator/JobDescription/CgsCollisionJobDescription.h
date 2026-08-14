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
//   stw   r26, 0xF8(r3)   ->  mpDebugStream  (debug render stream reader; null here)
//   stb   r10, 0xFF(r3)   ->  muJobType      (job-type id: 5/7/10/11 per concrete job)
//
// ⭐ 2026-08-14 (walls leg 1): the +0xF8 seat was previously modelled as `s32 miStatus`
// ("status/result count, init 0") — a MISNAME. The DecFIGS DWARF (CgsCollisionJobDescription
// .h:119) spells it `DebugRenderStreamReader * mpDebugStream`, and the three collide-stream
// Run* dispatchers STORE A REAL POINTER through it (RunCollideSphereListWithTriangleListStream
// @0x828115F4 `stw r22, 0x4C8(batch)` == the lpDebugReader argument; the swept twin likewise;
// the sphere-sphere twin and every non-stream Prepare store null). An s32 seat would have
// truncated the pointer on x64 the day the stream family landed — corrected WITH that family.
//
// SEMANTIC parity (member-by-name), not byte-match: the four bookkeeping fields sit at
// X360 offsets 0xF0..0xFF (after the derived 240-byte Data payload). Modelled here as
// named base members; the derived data lists are named members of each derived type.
// (Host pointer width differs from the X360's 4-byte pointers, so we do NOT pin the
// X360 256-byte sizeof — the recovered behaviour is the field writes, which we keep.)

#include "types.hpp"

// The debug render stream reader the collide-stream Run* dispatchers thread through the
// descriptor (pointer use only; home CgsDebugRenderStreamReader.h:23, class key `class`).
namespace CgsDev { class DebugRenderStreamReader; }

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

        // ⭐ 2026-08-14 (walls leg 1): the three collide-STREAM ids, written by the three
        // Run* dispatchers (`li r24, 6` @0x828115B4 / `li r23, 0xE` @swept / `li r23, 8`
        // @sphere-sphere -> `stb, 0x4CF(batch)`) and read back by ContactGeneratorJob::
        // Execute's 12-way switch (cases 6/8/14 -- the three named worker gates). The DWARF
        // enum (CgsCollisionJobDescription.h:53) spells them E_COLLISION_TYPE_SPHERE_
        // TRIANGLE_STREAM / SPHERE_SPHERE_STREAM / SWEPT_SPHERE_TRIANGLE_STREAM; this tree's
        // established E_COLLISIONJOB_ prefix is kept.
        E_COLLISIONJOB_SPHERE_LIST_WITH_TRIANGLE_LIST_STREAM = 6,   // Run @0x82811550
        E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST       = 7,   // @0x82810198
        E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST_STREAM = 8,  // Run @0x82811C00
        E_COLLISIONJOB_PRIMITIVE_PAIR_LIST                = 10,  // @0x82810478
        E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST  = 11,  // @0x82810278
        E_COLLISIONJOB_SWEPT_SPHERE_LIST_WITH_TRIANGLE_LIST_STREAM = 14, // Run @0x828118A8

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
        f32                  mfRadius;       // X360 +0xF4  contact radius / padding (0.0); DWARF name mfPadding
        CgsDev::DebugRenderStreamReader* mpDebugStream; // X360 +0xF8  DWARF h:119 (was misnamed `s32 miStatus` -- see banner)
        u8                   muJobType;      // X360 +0xFF  E_CollisionJobType id; DWARF name meType

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
