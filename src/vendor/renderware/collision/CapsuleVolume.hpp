#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/Feature.hpp"    // Vec4 / RwBool / FeatureEdge / Feature
#include "vendor/renderware/collision/AABBox.hpp"      // AABBox / math::vpu::Vector3 (GetBBox / GetBBoxDiag out)

// ===========================================================================
// rw::collision::CapsuleVolume -- the RenderWare collision "capsule volume":
// a local orientation frame (three basis rows + a centre/translation row) plus
// two scalar dimensions -- an axial "half-height" (+0x44, scales the oriented
// axis row) and a radial "radius" (+0x50, isotropic, added to every AABB axis)
// -- and a small block of state words. Its layout mirrors the sibling CylinderVolume
// (both are 0x60-class RenderWare volumes); only the field roles the groundable
// functions exercise differ, and those are documented per member below.
//
// OWNING HOME for the CapsuleVolume functions the X360 binary defines:
//     rw::collision::CapsuleVolume::GetBBoxDiag        @ 0x82BAF6A8   [homed]
//     rw::collision::CapsuleVolume::CreateGPInstance   @ 0x82BAF6F0   [homed]
//     rw::collision::CapsuleVolume::GetMaximumFeature  @ 0x82BAF808   [homed]
//     rw::collision::CapsuleVolume::GetBBox            @ 0x82BAF9C8   [homed]
//
// BLOCKED (not reconstructed here -- declaration omitted per the sibling
// TriangleVolume / CylinderVolume precedent):
//     rw::collision::CapsuleVolume::LineSegIntersect   @ 0x82BAFCF8
//        -- a 428-instruction register-allocated VMX line/segment narrow-phase
//           kernel (v120-v127 hand-allocated vector regs via
//           __savevmx_120/__restvmx_120, cr-bit predicate extraction, and
//           rwcSphereLineSegIntersect @0x82BA81D8 + rwcCylinderLineSegIntersect
//           @0x82BAF8A0 calls). The blocker is the register-allocated VMX
//           narrow-phase GEOMETRY only: its three .rdata scalars ARE recovered
//           (flt_82001CC0 == 0.0f -- also the value CylinderVolume::Initialize
//           stamps -- flt_82001C98 == 1.0f and flt_820037C8 == -1.0f, the zero
//           fill and the +/-1 arms of the axial-slab sign selector that
//           compares against +0x44) and are NOT what blocks it. A future wave
//           has a readable entry point: that slab test against mfHalfHeight,
//           the 3-iteration loop (li r24, 3 @0x82BAFD78, addic. r24, r24, -1
//           @0x82BB0088) and the sphere-cap-then-cylinder-barrel call order
//           all survive into the export's pseudocode.
//
// NO DWARF / Feb-2007 source exists for this TU. The LAYOUT below is entirely
// X360-asm-attested (member OFFSETS are ground truth, pinned by the
// static_asserts; the member NAMES are inferred and documented). The 16-byte
// vector rows are carried as the shared scalar rw::collision::Vec4, the same
// vocabulary FeatureEdge / Feature / CylinderVolume trade in.
// ===========================================================================

namespace rw
{
namespace collision
{

struct GPInstance;   // vendor/renderware/collision/GPInstance.hpp

class CapsuleVolume
{
public:
    // Canonical default ctor (members left uninitialised).
    CapsuleVolume() {}

    // @ 0x82BAF6F0 -- instance this capsule into the GP ("generalised
    // primitive") narrow-phase image arInst, optionally transformed by the
    // 4-row affine matrix lpTransform (NULL = local space). This is the
    // collision-volume vtable's CreateGPInstance slot (slot 5, +0x14 -- see
    // PrimitiveIntersect.cpp); the X360 register contract is the plain
    // __fastcall member one: r3 = this, r4 = &arInst, r5 = lpTransform.
    // The console always returns 1 (li r3, 1) and every caller discards it.
    RwBool CreateGPInstance(GPInstance& arInst, const Vec4* lpTransform) const;

    // @ 0x82BAF6A8 -- return the local axis-aligned bounding-box diagonal:
    // 2 * ( |maFrame[2]| * mfHalfHeight + mfRadius ) per component. X360
    // struct-return convention: r3 = the hidden 16-byte return slot, r4 = this.
    math::vpu::Vector3 GetBBoxDiag() const;

    // @ 0x82BAF808 -- build the capsule's maximum contact feature: the central
    // axis segment ( maFrame[3] +/- maFrame[1] * mfHalfHeight ) as a single
    // FeatureEdge in arResult.edges[0], numedges = 1. abCcw / arDir are accepted
    // but unused (r4 / r5 are dead in the asm). Structurally identical to
    // CylinderVolume::GetMaximumFeature; the axis is scaled by the +0x44 scalar
    // (mfHalfHeight).
    void GetMaximumFeature(RwBool abCcw, const Vec4& arDir, Feature& arResult) const;

    // @ 0x82BAF9C8 -- write the capsule's (optionally transformed) AABB to
    // arResult and return 1. The oriented axis is maFrame[2] and the centre is
    // maFrame[3]; when lpTransform != NULL both are transformed by the affine
    // matrix (axis by the 3x3 rotation, centre as a point) first. The half
    // extent is |axis| * mfHalfHeight + mfRadius per component; min = centre -
    // extent, max = centre + extent. abTight (r5) is accepted but DEAD in the
    // asm (unlike CylinderVolume::GetBBox, the capsule has no tight/loose split).
    RwBool GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const;

    // --- members (X360-asm-attested offsets; inferred names) ----------------
    // The local frame: three basis rows + a centre row. GetBBox / GetBBoxDiag /
    // CreateGPInstance read maFrame[2] as the oriented capsule axis and
    // maFrame[3] as the centre (CreateGPInstance rotates maFrame[2] by the
    // CALLER's transform rows and transforms maFrame[3] as a point -- it reads
    // nothing else from the frame); GetMaximumFeature reads maFrame[1] as the
    // (feature) axis and maFrame[3] as the centre.
    Vec4 maFrame[4];    // +0x00 / +0x10 / +0x20 / +0x30
    u32  mu40;          // +0x40  (unread by the homed funcs; init word slot,
                        //         cf. CylinderVolume::mInitWord)
    f32  mfHalfHeight;  // +0x44  axial half-length: scales |maFrame[2]| in the
                        //         AABB and the GetMaximumFeature segment, and is
                        //         copied to GPInstance::mDimensions.x (+0x70)
    u32  mu48;          // +0x48  (unread by the homed funcs; keeps +0x50 aligned)
    u32  mu4C;          // +0x4C  (unread by the homed funcs)
    f32  mfRadius;      // +0x50  radial extent: isotropic AABB add; copied to
                        //         GPInstance::mFatness (+0x80)
    u32  mu54;          // +0x54  (unread by the homed funcs)
    u32  mu58;          // +0x58  (unread by the homed funcs)
    u32  muFlags;       // +0x5C  volume flag word: copied verbatim into
                        //         GPInstance::mFlags (+0x94) by CreateGPInstance
};

static_assert(offsetof(CapsuleVolume, maFrame)     == 0x00, "CapsuleVolume::maFrame");
static_assert(offsetof(CapsuleVolume, mfHalfHeight)== 0x44, "CapsuleVolume::mfHalfHeight");
static_assert(offsetof(CapsuleVolume, mfRadius)    == 0x50, "CapsuleVolume::mfRadius");
static_assert(offsetof(CapsuleVolume, muFlags)     == 0x5C, "CapsuleVolume::muFlags");

} // namespace collision
} // namespace rw
