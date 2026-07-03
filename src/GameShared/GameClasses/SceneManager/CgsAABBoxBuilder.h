#pragma once

// CgsSceneManager::AABBoxBuilder -- the scene manager's AABB builders over
// RenderWare GP ("generalised primitive") instances: one entry per primitive
// family, feeding rw::collision::AABBox results into the scene-query pipeline.
// Stateless -- static members only.
//
// HOME for the 3-function TU (class:CgsSceneManager::AABBoxBuilder),
// reconstructed from BURNOUT_X360_ARTIST.XEX (dedicated VMX pass wave 2):
//   AABBoxBuilder::CreateFromTriangle  @ 0x82B57DA0
//   AABBoxBuilder::CreateFromBox       @ 0x82B57DE0
//   AABBoxBuilder::CreateFromCylinder  @ 0x82B57F20  (branch thunk onto Box)

#include "vendor/renderware/collision/GPInstance.hpp"

namespace rw
{
namespace collision
{
    class AABBox;   // vendor/renderware/collision/AABBox.hpp
}
}

namespace CgsSceneManager
{
    class AABBoxBuilder
    {
    public:
        // @ 0x82B57DA0 -- per-lane min/max fold of the three GP-triangle
        // vertex rows into the AABB's min/max rows.
        static void CreateFromTriangle(rw::collision::AABBox* lpBBox,
                                       const rw::collision::GPInstance* lpTriangle);

        // @ 0x82B57DE0 -- generic support-projection builder: project the
        // instance onto +X/+Y/+Z through its batched-interval callback
        // (GPInstance::mMethods.mGetIntervals) and pack the three intervals
        // into the AABB rows.
        static void CreateFromBox(rw::collision::AABBox* lpBBox,
                                  const rw::collision::GPInstance* lpVolume);

        // @ 0x82B57F20 -- pure branch thunk onto CreateFromBox (the interval
        // callback covers the cylinder primitive).
        static void CreateFromCylinder(rw::collision::AABBox* lpBBox,
                                       const rw::collision::GPInstance* lpVolume);
    };
}
