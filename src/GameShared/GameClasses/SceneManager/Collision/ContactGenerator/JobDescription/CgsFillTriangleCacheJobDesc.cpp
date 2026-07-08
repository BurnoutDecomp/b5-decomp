#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsFillTriangleCacheJobDesc.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
// CgsSceneManager::CgsCollision::FillTriangleCacheJobDesc accessors, called by
// PolygonSoupTesterJob::ExecuteFillTriangleCache (@0x82915AE0).
//
// Each getter is emitted out-of-line and calls an ICF-folded "return this" accessor (a
// bare `blr`, folded with the empty BaseCollisionGenerator::Destruct @0x8284CB38) before
// reading its member — i.e. the inlined `GetData()`-style accessor collapsed to the
// identity function. Modelled here as direct named-member access (semantic parity).

namespace CgsSceneManager
{
namespace CgsCollision
{
    // GetCacheSph @0x82916EF0:  bl <return this>; blr  — returns `this`, which is the
    // address of the leading cache sphere member.
    const CgsGeometric::Sphere* FillTriangleCacheJobDesc::GetCacheSphere() const
    {
        return &mCacheSphere;
    }

    // GetTriangleLis @0x82916F18:  bl <return this>; lwz r3,0x14(r3); blr
    TriangleList* FillTriangleCacheJobDesc::GetTriangleList() const
    {
        return mpTriangleList;
    }

    // GetMaxNumTriangles @0x82916F48:  bl <return this>; lhz r11,0x18(r3); mr r3,r11; blr
    u16 FillTriangleCacheJobDesc::GetMaxNumTriangles() const
    {
        return mu16MaxNumTriangles;
    }
}
}
