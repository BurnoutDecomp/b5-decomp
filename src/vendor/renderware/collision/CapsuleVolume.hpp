#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "SDKs/EATech/rwcollision/volume_debug_access.h"
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
// LANDED 2026-09-25 (crash parity FX-FOLLOWUPS stage (b)): LineSegIntersect @0x82BAFCF8 is declared below; its body
// is in LineSegIntersect.cpp, beside the rwc* kernels it calls (the BLOCKED note above predates it). Its signature is
// the DWARF's -- DecFIGS dwarfdump .../rw/collision/capsule.h:144 -- so DWARF does exist for this class, whatever the
// next line says.
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
struct VolumeLineSegIntersectResult;   // vendor/renderware/collision/LineSegIntersect.hpp

class CapsuleVolume : public Volume
{
public:
    // Canonical default ctor (members left uninitialised).
    CapsuleVolume() {}

    RwBool GetInterval(const Vector3& arDir, Interval& arInterval) const;
    void Release();

    // @ 0x82BAF6F0 -- instance this capsule into the GP ("generalised
    // primitive") narrow-phase image arInst, optionally transformed by the
    // 4-row affine matrix lpTransform (NULL = local space). This is the
    // collision-volume vtable's CreateGPInstance slot (slot 5, +0x14 -- see
    // PrimitiveIntersect.cpp); the X360 register contract is the plain
    // __fastcall member one: r3 = this, r4 = &arInst, r5 = lpTransform.
    // The console always returns 1 (li r3, 1) and every caller discards it.
    RwBool CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const;

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
    RwBool GetMaximumFeature(RwBool abCcw, const Vector3& arDir, Feature& arResult) const;

    // @ 0x82BAF9C8 -- write the capsule's (optionally transformed) AABB to
    // arResult and return 1. The oriented axis is maFrame[2] and the centre is
    // maFrame[3]; when lpTransform != NULL both are transformed by the affine
    // matrix (axis by the 3x3 rotation, centre as a point) first. The half
    // extent is |axis| * mfHalfHeight + mfRadius per component; min = centre -
    // extent, max = centre + extent. abTight (r5) is accepted but DEAD in the
    // asm (unlike CylinderVolume::GetBBox, the capsule has no tight/loose split).
    RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const;

    // @ 0x82BAFCF8 (428 insns) -- DWARF capsule.h:144 `RwBool LineSegIntersect(const Vector3&, const Vector3&,
    // const Matrix44Affine*, VolumeLineSegIntersectResult&, float32_t) const`. The segment arPt1 -> arPt2 against
    // the capsule (the frame's z segment +/- mfHalfHeight, rounded by mfRadius + afFatness), in its frame
    // (composed with lpTransform when given); fills arResult on a hit (1). LANDED 2026-09-25 (FX-FOLLOWUPS
    // stage (b)); body in LineSegIntersect.cpp, bound to the descriptor's lineSegIntersect slot in VolumeVTables.cpp.
    RwBool LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                            VolumeLineSegIntersectResult& arResult, f32 afFatness) const;


};

CGS_STATIC_ASSERT((sizeof(CapsuleVolume) == 0x60), "Native CapsuleVolume size");

} // namespace collision
} // namespace rw
