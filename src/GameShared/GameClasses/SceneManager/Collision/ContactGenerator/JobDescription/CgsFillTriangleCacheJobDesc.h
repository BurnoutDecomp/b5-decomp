#pragma once

// CgsSceneManager::CgsCollision::FillTriangleCacheJobDesc — the job descriptor that
// drives PolygonSoupTesterJob::ExecuteFillTriangleCache (@0x82915AE0): it hands the job
// a query "cache sphere", the polygon-soup spatial map to walk, the destination triangle
// list to fill, and the cap on how many triangles to collect.
//
// Like the sibling contact-generator descriptors it derives from CollisionJobDescription
// (shared results-list / status / job-type bookkeeping) and carries its own typed payload
// at the front of the object. Semantic parity by NAMED MEMBER — the X360 uses 4-byte
// pointers and places the base bookkeeping at +0xF0 after the payload; the PC compile does
// NOT reproduce those byte offsets (pointers widen), so members are modelled in their
// attested order.
//
// Payload layout proven by the accessor asm (this=r3; each getter calls an ICF-folded
// "return this" accessor — the empty BaseCollisionGenerator::Destruct @0x8284CB38 / the
// twin @0x829170E8, both a bare `blr` — then reads the member):
//   GetCacheSph        @0x82916EF0  return this            -> &mCacheSphere      (+0x00)
//   (GetSpatialMap)    @0x82916EC0  return *(this+?)       ->  mpSpatialMap      (+0x10)
//   GetTriangleList    @0x82916F18  lwz r3,0x14(this)      ->  mpTriangleList    (+0x14)
//   GetMaxNumTriangles @0x82916F48  lhz r3,0x18(this)      ->  mu16MaxNumTriangles(+0x18)
// The cache sphere is a 16-byte CgsGeometric::Sphere (0x00..0x0F); the 4-byte slot at
// +0x10 holds the spatial-map pointer (attested by the descriptor's spatial-map getter and
// the caller's "No spacial map" assert). GetSpatialMap is a separate ledger function, not
// reconstructed here.

#include "types.hpp"

#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h" // CollisionJobDescription (base)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                                                     // CgsGeometric::Sphere (by value)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsTriangleList.h"                                  // TriangleList (destination, by pointer)

namespace CgsGeometric
{
    // Polygon-soup spatial map the job walks; referenced only by pointer here. Full home:
    // GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h.
    struct PolygonSoupListSpatialMap;
}

namespace CgsSceneManager
{
namespace CgsCollision
{
    struct FillTriangleCacheJobDesc : public CollisionJobDescription
    {
        // --- payload (front of the object; base bookkeeping trails at +0xF0) ----------
        CgsGeometric::Sphere               mCacheSphere;         // +0x00  query cache sphere
        CgsGeometric::PolygonSoupListSpatialMap* mpSpatialMap;   // +0x10  soup spatial map
        TriangleList*                      mpTriangleList;       // +0x14  destination list
        u16                                mu16MaxNumTriangles;  // +0x18  triangle cap

        // GetCacheSph @0x82916EF0 — pointer to the query cache sphere (the first member,
        // so the asm returns `this` unchanged).
        const CgsGeometric::Sphere* GetCacheSphere() const;

        // GetTriangleLis @0x82916F18 — the destination triangle list the job fills.
        TriangleList* GetTriangleList() const;

        // GetMaxNumTriangles @0x82916F48 — cap on triangles collected (u16, lhz).
        u16 GetMaxNumTriangles() const;
    };
}
}
