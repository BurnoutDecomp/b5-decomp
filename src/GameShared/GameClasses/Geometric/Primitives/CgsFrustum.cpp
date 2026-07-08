#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// ============================================================================
// CgsGeometric::Frustum -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Reconstructed here:
//   CgsGeometric::Frustum::VectorToPlane          @ 0x82840DB0
//   CgsGeometric::Frustum::GetPlaneByIndex        @ 0x8274EFE8
//   CgsGeometric::Frustum::SetPlaneByIndex        @ 0x827BAA48
//   CgsGeometric::Frustum::IsSphereInFrustum      @ 0x828AF020
//
// The stored planes live in a struct-of-arrays layout across the 8 x 16-byte
// maSwizzledPlanes lanes: two batches of four planes, each batch four lanes
//   +0x00 Nx  +0x10 Ny  +0x20 Nz  +0x30 offset   (planes 0..3, one per float)
//   +0x40 Nx  +0x50 Ny  +0x60 Nz  +0x70 offset   (planes 4..7, one per float)
// -- proven by the fixed load offsets in IsSphereInFrustum. The X360 gathers /
// scatters a single plane with lvsl-built vperm control vectors (rodata permute
// masks); those masks are the SIMD encoding of "select float lane (index & 3)",
// so they lower to the per-lane component moves below (the same VMX-de-swizzling
// precedent as CgsLineTests.cpp / CgsTriangleBox.cpp). Get/Set share one lane
// mapping, so the round-trip is exact regardless of the intra-batch labelling.
//
// Sibling ledger functions in their own passes (their bodies need collaborators
// this file cannot ground faithfully):
//   CgsGeometric::Frustum::SetFromRwFrustum       @ 0x82839FA8  (BLOCKED)
//   CgsGeometric::Frustum::DebugRender            @ 0x82845EC0  (BLOCKED)
//   CgsGeometric::Frustum::DebugRenderCustomPlanes@ 0x82845BB8  (BLOCKED)
//
// The BLOCKED trio depend on collaborators that cannot be grounded here:
//   - SetFromRwFrustum transposes the 6 (negated) input planes into the 8
//     swizzled SoA lanes with eight `vperm` control vectors loaded from rodata
//     (unk_82CDA3F0, unk_82CDADB0/C0/D0/E0/F0, unk_82CDB430/450). Those permute
//     masks are NOT in the dossier, and the exact AoS->SoA lane mapping (which
//     the culling tests read back) cannot be reproduced without them.
//   - DebugRender / DebugRenderCustomPlanes draw the frustum through the 3D
//     debug renderer (CgsDev::DebugRender::DrawQuad / DrawArrow / DrawLine),
//     whose declarations/arg shapes are not homed, and they call the still-todo
//     sibling method IntersectionOf3Planes and the un-pinned rw::collision::Plane
//     accessors (GetNormal / GetDistance).
// ============================================================================

namespace CgsGeometric
{
    namespace
    {
        // The 8 stored lanes are a struct-of-arrays: maSwizzledPlanes[batch + comp]
        // packs component `comp` of a batch's four planes across the float lanes,
        // one plane per lane. Selecting plane (index & 3)'s component is picking the
        // (index & 3)-th float -- the X360 does it with an lvsl-built vperm; these
        // are the de-swizzled per-lane forms.
        inline f32 LaneGet(const Vector4& lrLane, u32 luLane)
        {
            switch (luLane)
            {
            case 0:  return lrLane.x;
            case 1:  return lrLane.y;
            case 2:  return lrLane.z;
            default: return lrLane.w;
            }
        }

        inline void LaneSet(Vector4& lrLane, u32 luLane, f32 lfValue)
        {
            switch (luLane)
            {
            case 0:  lrLane.x = lfValue; break;
            case 1:  lrLane.y = lfValue; break;
            case 2:  lrLane.z = lfValue; break;
            default: lrLane.w = lfValue; break;
            }
        }
    }

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

    // ------------------------------------------------------------------------
    // GetPlaneByIndex @ 0x8274EFE8
    //
    // r3 = sret Plane buffer, r4 = this, r5 = luPlaneIndex. Asserts index < 8,
    // then, on the batch chosen by (index >= 4), builds a permute from
    // lvsl(4 * (index & 3)) and gathers the four SoA component lanes:
    //   index < 4 : lanes at this+0x00 / +0x10 / +0x20 / +0x30
    //   index >= 4: lanes at this+0x40 / +0x50 / +0x60 / +0x70
    // The four broadcast components are packed [Nx, Ny, Nz, offset] into an AoS
    // vector and handed to VectorToPlane. Lowered to the per-lane gather below.
    // ------------------------------------------------------------------------
    rw::collision::Plane Frustum::GetPlaneByIndex(u32 luPlaneIndex) const
    {
        CGS_ASSERT(luPlaneIndex < 8, "luPlaneIndex < 8");

        const u32 luLane  = luPlaneIndex & 3;
        const u32 luBatch = (luPlaneIndex >= 4) ? 4u : 0u;   // 4 SoA lanes per batch

        Vector4 lPlaneVector;
        lPlaneVector.x = LaneGet(maSwizzledPlanes[luBatch + 0], luLane);   // Nx
        lPlaneVector.y = LaneGet(maSwizzledPlanes[luBatch + 1], luLane);   // Ny
        lPlaneVector.z = LaneGet(maSwizzledPlanes[luBatch + 2], luLane);   // Nz
        lPlaneVector.w = LaneGet(maSwizzledPlanes[luBatch + 3], luLane);   // signed offset
        return VectorToPlane(lPlaneVector);
    }

    // ------------------------------------------------------------------------
    // SetPlaneByIndex @ 0x827BAA48
    //
    // r3 = this, r4 = luPlaneIndex, r5 = lrPlane. Asserts index < 8, packs the
    // plane into its stored (negated) AoS form via PlaneToVector, then, using a
    // per-lane insert mask keyed by ((index & 3) << 6), scatters each component
    // into float lane (index & 3) of the matching SoA vector -- the batch chosen
    // by (index >= 4), the same +0x00/+0x40 split GetPlaneByIndex reads back.
    // The register left in r3 by the asm is leftover mask-address scheduling, not
    // a source-level return value; the setter returns void.
    // ------------------------------------------------------------------------
    void Frustum::SetPlaneByIndex(u32 luPlaneIndex, const rw::collision::Plane& lrPlane)
    {
        CGS_ASSERT(luPlaneIndex < 8, "luPlaneIndex < 8");

        const Vector4 lPlaneVector = PlaneToVector(lrPlane);

        const u32 luLane  = luPlaneIndex & 3;
        const u32 luBatch = (luPlaneIndex >= 4) ? 4u : 0u;

        LaneSet(maSwizzledPlanes[luBatch + 0], luLane, lPlaneVector.x);   // Nx
        LaneSet(maSwizzledPlanes[luBatch + 1], luLane, lPlaneVector.y);   // Ny
        LaneSet(maSwizzledPlanes[luBatch + 2], luLane, lPlaneVector.z);   // Nz
        LaneSet(maSwizzledPlanes[luBatch + 3], luLane, lPlaneVector.w);   // signed offset
    }

    // ------------------------------------------------------------------------
    // IsSphereInFrustum @ 0x828AF020
    //
    // r3 = this, r4 = sphere ([cx, cy, cz, radius] in one lane). Splats the
    // centre (x/y/z) and radius, then runs the two SoA batches in parallel:
    //   distance = Nx*cx + Ny*cy + Nz*cz - offset            (per plane)
    //   outside  = distance > radius                          (vcmpgtfp)
    // The per-lane b0/b1 outside masks are OR'd, mapped to 1.0/0.0 (vsel), and
    // vcmpeqfp. against 0.0 sets CR6[all-true] iff every plane keeps the sphere
    // inside -- the bit the function returns. Re-rolled over the 4 SoA lanes; a
    // NaN distance compares not-greater (matching vcmpgtfp), so it never rejects.
    // ------------------------------------------------------------------------
    bool Frustum::IsSphereInFrustum(const Sphere& lrSphere) const
    {
        const f32 lfCx     = lrSphere.mPositionRadius.x;
        const f32 lfCy     = lrSphere.mPositionRadius.y;
        const f32 lfCz     = lrSphere.mPositionRadius.z;
        const f32 lfRadius = lrSphere.mPositionRadius.w;

        for (u32 luLane = 0; luLane < 4; ++luLane)   // 4 lanes x 2 batches = 8 planes
        {
            const f32 lfDistance0 =
                  LaneGet(maSwizzledPlanes[0], luLane) * lfCx
                + LaneGet(maSwizzledPlanes[1], luLane) * lfCy
                + LaneGet(maSwizzledPlanes[2], luLane) * lfCz
                - LaneGet(maSwizzledPlanes[3], luLane);

            const f32 lfDistance1 =
                  LaneGet(maSwizzledPlanes[4], luLane) * lfCx
                + LaneGet(maSwizzledPlanes[5], luLane) * lfCy
                + LaneGet(maSwizzledPlanes[6], luLane) * lfCz
                - LaneGet(maSwizzledPlanes[7], luLane);

            if (lfDistance0 > lfRadius || lfDistance1 > lfRadius)
                return false;
        }
        return true;
    }
}
