#pragma once

// ============================================================================
// GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h
//
// Declaration home for the sphere-vs-Triangle4 CONTACT kernels of
// CgsTriangleSphere.cpp (DecFIGS DWARF homes this source file and both
// signatures verbatim -- dwarfdump CgsTriangleSphere.cpp:490/:715).
//
// Reconstructed this pass (walls leg 2, 2026-08-14):
//   CgsGeometric::IntersectTriangle4Sphere_HackyBurnoutVersion @0x8283D2E0 (497)
//
// Sibling NOT reconstructed (no X360 caller on the vehicle path; the PS3 ELF
// carries it @0xB59F6C):
//   CgsGeometric::IntersectTriangle4Sphere                     (DWARF :490)
//
// The 19-parameter shape is the DWARF's own: sphere, triangle batch, a
// broadcast padding lane, then FOUR GROUPS of output references -- one group
// per SoA lane -- each {ContactNormal, TriangleNormal, SphereContactPoint,
// TriangleContactPoint}. The PS3 mangle spells the group order:
//   _ZN12CgsGeometric44IntersectTriangle4Sphere_HackyBurnoutVersionE
//   RKNS_6SphereERKNS_9Triangle4EN2rw4math3vpu8VecFloatE
//   RNS8_7Vector3ESB_RNS8_11Vector3PlusESD_  (x4)
// and the X360 call site (ExecuteSphereListWithTriangleList @0x829226A8)
// passes exactly these, six in r5..r10 and ten on the stack.
// ============================================================================

#include "BrnCommonTypes.h"   // Vector3 / Vector3Plus / VecFloat (rw::math::vpu aliases)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"

namespace CgsGeometric
{
    struct Sphere;

    // The minimum signed plane distance of the sphere centre above a triangle's
    // face plane for a contact to be considered at all. ⚠️ DYNAMIC-INIT on the
    // console: ZERO in the image (unk_83039220), written at static-init time by
    // sub_82C6DCF0 = CgsNumeric::CreateFloatVector(0.001f); the PS3 initializer
    // (__static_initialization_and_destruction_0 @0xB58360) builds the same
    // 0.001f splat into CgsGeometric::KF_MIN_PLANE_DIST. Both consoles agree.
    extern const VecFloat KF_MIN_PLANE_DIST;

    // IntersectTriangle4Sphere_HackyBurnoutVersion @0x8283D2E0 (497)
    //
    // Sphere vs four SoA triangles. Per lane: closest point on the triangle to
    // the sphere centre (face plane projection, else the LAST violated edge's
    // clamped segment point in edge order P0->P1, P1->P2, P2->P0), then
    //   TriangleContactPoint = that closest point
    //   ContactNormal == TriangleNormal = normalize(centre - closest)
    //     (the face normal for a face contact; the true direction on edges --
    //      the console stores the SAME register to both outputs)
    //   SphereContactPoint   = centre - normal * radius
    // and the hit mask lane is
    //   valid-lane & planeDist >= KF_MIN_PLANE_DIST & dist <= radius + padding
    //   & (face contact | dot(normal, faceNormal) >= 0)
    //   & per violated edge i: dot(normal, faceNormal) >= mEdge<i>Cosigns lane
    // The edge-cosine rows (+0xB0/+0xC0/+0xD0, LoadEdgeCosines' product) are the
    // "Hacky" part: internal-edge filtering so a wall's tessellation seams do
    // not emit ghost-edge contacts.
    //
    // Derivation provenance (walls leg 2): the 497 straight-line VMX128
    // instructions were decoded from the image words (78 IDA "+32" source-field
    // misprints corrected) and executed numerically; this scalar lowering was
    // then fuzz-compared against that execution -- 600 single-lane + 2000
    // distinct-triangle lane trials, 0 mismatches (scratchpad sim_kernel.py /
    // fuzz_kernel.py, wallsleg2_log.md).
    Triangle4::Mask4 IntersectTriangle4Sphere_HackyBurnoutVersion(
        const Sphere&    lSphere,
        const Triangle4& lTriangles,
        VecFloat         lInPadding,
        Vector3& lContactNormal0, Vector3& lTriangleNormal0,
        Vector3Plus& lSphereContactPoint0, Vector3Plus& lTriangleContactPoint0,
        Vector3& lContactNormal1, Vector3& lTriangleNormal1,
        Vector3Plus& lSphereContactPoint1, Vector3Plus& lTriangleContactPoint1,
        Vector3& lContactNormal2, Vector3& lTriangleNormal2,
        Vector3Plus& lSphereContactPoint2, Vector3Plus& lTriangleContactPoint2,
        Vector3& lContactNormal3, Vector3& lTriangleNormal3,
        Vector3Plus& lSphereContactPoint3, Vector3Plus& lTriangleContactPoint3);
}
