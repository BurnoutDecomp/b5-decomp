#ifndef CGS_SPHERE_H
#define CGS_SPHERE_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 / VecFloat aliases (rw::math::vpu)

// ============================================================================
// GameShared/GameClasses/Geometric/Primitives/CgsSphere.h
//
// CgsGeometric::Sphere -- a bounding sphere packed into a single 16-byte vector:
// the centre position in the x/y/z lanes and the radius in the w lane. OWNING
// home reconstructed from the X360 ARTIST build.
//
// LAYOUT (proven by the asm):
//   GetPosition @ 0x825B27F8   lvx128 v0, this; stvx128 v0, out   (whole vector)
//   GetRadius   @ 0x825BD1F8   lvx128 v0, this; vspltw v0,v0,3    (lane 3 = w)
//
//   +0x00  Vector4 mPositionRadius   xyz = centre, w = radius
//   sizeof == 16
//
// Member name INFERRED from the position/radius access pattern (the DWARF has
// no entry for this TU); the single-vector shape is proven by the lone +0x00
// 16-byte load in both getters and the lane-3 splat that extracts the radius.
// ============================================================================

namespace CgsGeometric
{
    struct Sphere
    {
        Vector4 mPositionRadius;   // +0x00  xyz = centre position, w = radius

        // GetPosition @ 0x825B27F8 -- return the whole packed vector (the asm
        // copies all four lanes verbatim; xyz is the centre, w trails along).
        // The X360 ABI returns the 16-byte value via a hidden sret pointer (r3).
        Vector4 GetPosition() const;

        // GetRadius @ 0x825BD1F8 -- return the radius broadcast across all four
        // lanes (vspltw v0,v0,3 splats the w lane). The result is a VecFloat
        // (one float in every lane), again returned via the sret pointer.
        VecFloat GetRadius() const;
    };
}

#endif // CGS_SPHERE_H
