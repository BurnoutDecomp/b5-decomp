#pragma once

// CgsGeometric::Frustum — a 6-plane view frustum stored as 8 swizzled plane lanes for the
// SoA culling tests (the 8th/extra lanes pad the SoA batch). Layout recovered from the DecFIGS
// DWARF (CgsFrustum.h:46/159): a single Vector4[8] maSwizzledPlanes member -> exactly 128 bytes
// (0x80), the size FrustumJobQueryInfo::operator= block-copies per maFrustum[] element.
//
// Only the data layout is provided here (the type is embedded by value in
// CgsSceneManager::FrustumJobQueryInfo); the Frustum member functions the X360 defines
// (GetPlane/SetPlane/... CgsFrustum.h:86+) are out-of-scope separate TUs.

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector4 (16-byte SIMD lane)

namespace CgsGeometric
{
    struct Frustum
    {
        // CgsFrustum.h:50 (DWARF) — plane index identifiers.
        enum PlaneId
        {
            PlaneLeft   = 0,
            PlaneTop    = 1,
            PlaneRight  = 2,
            PlaneBottom = 3,
            PlaneFar    = 4,
            PlaneNear   = 5,
        };

        // CgsFrustum.h:159 (DWARF). 8 swizzled plane lanes = 128 bytes.
        Vector4 maSwizzledPlanes[8];
    };
}
