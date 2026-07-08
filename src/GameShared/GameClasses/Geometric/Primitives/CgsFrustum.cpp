#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"

// ============================================================================
// CgsGeometric::Frustum -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// This TU carries four ledger functions:
//   CgsGeometric::Frustum::VectorToPlane          @ 0x82840DB0  (below)
//   CgsGeometric::Frustum::SetFromRwFrustum       @ 0x82839FA8  (BLOCKED)
//   CgsGeometric::Frustum::DebugRender            @ 0x82845EC0  (BLOCKED)
//   CgsGeometric::Frustum::DebugRenderCustomPlanes@ 0x82845BB8  (BLOCKED)
//
// Only VectorToPlane is reconstructed here. The other three are left to their
// own passes because they depend on collaborators this pass cannot ground
// faithfully:
//   - SetFromRwFrustum transposes the 6 (negated) input planes into the 8
//     swizzled SoA lanes with eight `vperm` control vectors loaded from rodata
//     (unk_82CDA3F0, unk_82CDADB0/C0/D0/E0/F0, unk_82CDB430/450). Those permute
//     masks are NOT in the dossier, and the exact AoS->SoA lane mapping (which
//     the culling tests read back) cannot be reproduced without them.
//   - DebugRender / DebugRenderCustomPlanes draw the frustum through the 3D
//     debug renderer (CgsDev::DebugRender::DrawQuad / DrawArrow / DrawLine),
//     whose declarations/arg shapes are not homed, and they call the still-todo
//     sibling methods GetPlaneByIndex + IntersectionOf3Planes and the
//     un-pinned rw::collision::Plane accessors (GetNormal / GetDistance).
// ============================================================================

namespace CgsGeometric
{
    // ------------------------------------------------------------------------
    // VectorToPlane @ 0x82840DB0
    //
    //   vspltisw v0, -1        ; v0 = 0xFFFFFFFF per lane
    //   lvx128   v13, r0, r5   ; v13 = *lrVector  (the packed swizzled lane; r4
    //                          ;                    == this is unused)
    //   vslw     v0, v0, v0    ; v0 = 0xFFFFFFFF << 31 = 0x80000000 (sign mask)
    //   vxor     v0, v13, v0   ; flip the sign bit of all four lanes -> negate
    //   stvx128  v0, r0, r3    ; *result = negated vector, reinterpreted as Plane
    //   blr
    //
    // The stored frustum lane is the negation of the plane it represents, so
    // recovering the plane is a whole-vector negate. `this` is never touched --
    // the method is const and ignores it (the stack lfs/stfs shuffle in the asm
    // is just the compiler's 16-byte copy of the negated result into the return
    // slot). Lowered to a clean per-lane arithmetic negation.
    // ------------------------------------------------------------------------
    rw::collision::Plane Frustum::VectorToPlane(const Vector4& lrVector) const
    {
        Vector4 lNegated;
        lNegated.x = -lrVector.x;
        lNegated.y = -lrVector.y;
        lNegated.z = -lrVector.z;
        lNegated.w = -lrVector.w;
        return rw::collision::Plane(lNegated);
    }
}
