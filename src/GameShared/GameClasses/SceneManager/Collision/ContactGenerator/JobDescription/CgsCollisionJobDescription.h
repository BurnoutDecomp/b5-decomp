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
        E_COLLISIONJOB_SPHERE_LIST_WITH_TRIANGLE_LIST     = 5,   // @0x82810100
        E_COLLISIONJOB_SPHERE_LIST_WITH_SPHERE_LIST       = 7,   // @0x82810198
        E_COLLISIONJOB_PRIMITIVE_PAIR_LIST                = 10,  // @0x82810478
        E_COLLISIONJOB_PRIMITIVE_LIST_WITH_TRIANGLE_LIST  = 11,  // @0x82810278
    };

    struct CollisionJobDescription
    {
        CollisionResultList* mpResultsList;  // X360 +0xF0  results list the job writes
        f32                  mfRadius;       // X360 +0xF4  contact radius / padding (0.0)
        s32                  miStatus;       // X360 +0xF8  status / result count (init 0)
        u8                   muJobType;      // X360 +0xFF  E_CollisionJobType id
    };
}
}
