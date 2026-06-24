#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"

// ============================================================================
// CgsGeometric::Sphere -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   CgsGeometric::Sphere::GetPosition @ 0x825B27F8
//   CgsGeometric::Sphere::GetRadius   @ 0x825BD1F8
//
// Both bodies are trivial single-vector loads with a structure return (the
// caller passes the out-buffer in r3, `this` in r4). No math -- pure data
// access; lowered here to the named single-vector member.
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // GetPosition @ 0x825B27F8
    //
    //   lvx128  v0, r0, r4      ; v0 = *(this+0x00)   (load the 16-byte vector)
    //   stvx128 v0, r0, r3      ; *out = v0           (store all four lanes)
    //   blr
    //
    // Returns the packed centre/radius vector unchanged (all four lanes copied).
    // ------------------------------------------------------------------------
    Vector4 Sphere::GetPosition() const
    {
        return mPositionRadius;
    }

    // ------------------------------------------------------------------------
    // GetRadius @ 0x825BD1F8
    //
    //   lvx128  v0, r0, r4      ; v0 = *(this+0x00)
    //   vspltw  v0, v0, 3       ; broadcast lane 3 (the w / radius lane) to all
    //   stvx128 v0, r0, r3      ; *out = v0
    //   blr
    //
    // Extracts the radius (the w lane) and broadcasts it across all four lanes,
    // returning it as a VecFloat (one float replicated per lane).
    // ------------------------------------------------------------------------
    VecFloat Sphere::GetRadius() const
    {
        const f32 lfRadius = mPositionRadius.w;   // vspltw v0,v0,3 -> lane 3
        VecFloat lvResult;
        lvResult.x = lfRadius;
        lvResult.y = lfRadius;
        lvResult.z = lfRadius;
        lvResult.w = lfRadius;
        return lvResult;
    }
}
