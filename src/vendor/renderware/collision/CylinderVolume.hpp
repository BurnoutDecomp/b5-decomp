#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/Feature.hpp"    // Vec4 / RwBool / FeatureEdge / Feature
#include "vendor/renderware/collision/AABBox.hpp"      // AABBox / math::vpu::Vector3 (GetBBox / GetBBoxDiag out)

// ===========================================================================
// rw::collision::CylinderVolume -- the RenderWare collision "cylinder volume":
// a local orientation frame (three basis rows + a centre/translation row) plus
// the three cylinder dimensions (half-height, radius, surface fatness) and a
// small block of state words. A deformable wheel is modelled as one of these
// (rw::collision::CylinderVolume::Initialize is called by
// BrnPhysics::Deformation::PhysicalWheel::AddToScene).
//
// OWNING HOME for the CylinderVolume functions the X360 binary defines:
//     rw::collision::CylinderVolume::Initialize          @ 0x82BAD3F0
//     rw::collision::CylinderVolume::GetBBoxDiag         @ 0x82BAC7B8
//     rw::collision::CylinderVolume::GetBBox             @ 0x82BAD490
//     rw::collision::CylinderVolume::GetMaximumFeature   @ 0x82BAC988
//
// BLOCKED (not reconstructed here -- see CylinderVolume.cpp for the precise
// reasons; declarations omitted per the TriangleVolume precedent):
//     rw::collision::CylinderVolume::CreateGPInstance    @ 0x82BAC7F8
//        -- installs the un-homed GP-cylinder VolumeMethods table
//           unk_82F918F0[5] (four method pointers copied into the GPInstance);
//           that runtime-dispatch pointer table is un-recovered rodata.
//     rw::collision::CylinderVolume::FatLineSegIntersect  @ 0x82BAEB10
//     rw::collision::CylinderVolume::ThinLineSegIntersect @ 0x82BADCE0
//        -- ~500-instruction register-allocated VMX line/segment narrow-phase
//           kernels (cr-bit predicate extraction, AALineClipper +
//           rwc{Cylinder,Torus}LineSegIntersect calls); a faithful de-optimised
//           reconstruction of the per-lane geometry cannot be grounded with
//           confidence, so they are left BLOCKED per the no-fabrication rule.
//
// NO DWARF / Feb-2007 source exists for this TU. The LAYOUT below is entirely
// X360-asm-attested (member OFFSETS are ground truth, pinned by the
// static_asserts; the member NAMES are inferred and documented). The 16-byte
// vector rows are carried as the shared scalar rw::collision::Vec4, the same
// vocabulary FeatureEdge / Feature / TriangleVolume trade in.
// ===========================================================================

namespace rw
{
namespace collision
{

class CylinderVolume
{
public:
    // Canonical default ctor (members left uninitialised until Initialize).
    CylinderVolume() {}

    // @ 0x82BAD3F0 -- stamp the cylinder's default local frame + the three
    // dimensions into the volume referenced by *appVolume and return it (NULL
    // when the slot is empty). X360 __fastcall: r3 = the slot holding the volume
    // pointer (`lwz r11, 0(r3)`); f1 = radius, f2 = half-height, f3 = fatness.
    static CylinderVolume* Initialize(CylinderVolume** appVolume,
                                      f32 afRadius, f32 afHalfHeight, f32 afFatness);

    // @ 0x82BAC7B8 -- return the local axis-aligned bounding-box diagonal,
    // 2 * { radius, radius, half-height }. X360 struct-return convention: r3 =
    // the hidden 16-byte return slot, r4 = this (const).
    math::vpu::Vector3 GetBBoxDiag() const;

    // @ 0x82BAD490 -- write the cylinder's (optionally transformed) AABB to
    // arResult and return 1. lpTransform NULL: the fattened local box centred at
    // the origin. lpTransform != NULL: box centred at the transform translation
    // (row 3); abTight selects a tight oriented extent (row 2 is the world axis)
    // vs a loose bounding-sphere extent.
    RwBool GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const;

    // @ 0x82BAC988 -- build the cylinder's maximum contact feature: the central
    // axis segment (centre +/- axis * half-height) as a single FeatureEdge in
    // arResult.edges[0], numedges = 1. abCcw / arDir are accepted but unused
    // (r4 / r5 are dead in the asm; the cylinder axis is always its maximum
    // feature here).
    void GetMaximumFeature(RwBool abCcw, const Vec4& arDir, Feature& arResult) const;

    // --- members (X360-asm-attested offsets; inferred names) ----------------
    // The local frame: three basis rows + a centre row. Initialize seeds
    // maFrame[0] = gIVector, maFrame[1]/[2] = the un-recovered axis-seed
    // constants unk_82181510 / unk_82181520, maFrame[3] = 0. GetMaximumFeature
    // reads maFrame[1] as the cylinder axis and maFrame[3] as the centre;
    // CreateGPInstance treats all four as the GP-instance transform rows.
    Vec4 maFrame[4];    // +0x00 / +0x10 / +0x20 / +0x30
    u32  mInitWord;     // +0x40  Initialize's dword_8327EEF4 (see .cpp)
    f32  mfHalfHeight;  // +0x44  cylinder half-height (axial extent)
    f32  mfRadius;      // +0x48  cylinder radius (radial extent)
    u32  mu4C;          // +0x4C  (unread by this TU; keeps +0x50 aligned)
    f32  mfFatness;     // +0x50  surface fatness (0 by default)
    u32  mu54;          // +0x54  (Initialize: 0)
    u32  mu58;          // +0x58  (Initialize: 0)
    u32  mu5C;          // +0x5C  state/type word (Initialize: 1)
};

static_assert(offsetof(CylinderVolume, maFrame)     == 0x00, "CylinderVolume::maFrame");
static_assert(offsetof(CylinderVolume, mInitWord)   == 0x40, "CylinderVolume::mInitWord");
static_assert(offsetof(CylinderVolume, mfHalfHeight)== 0x44, "CylinderVolume::mfHalfHeight");
static_assert(offsetof(CylinderVolume, mfRadius)    == 0x48, "CylinderVolume::mfRadius");
static_assert(offsetof(CylinderVolume, mfFatness)   == 0x50, "CylinderVolume::mfFatness");
static_assert(offsetof(CylinderVolume, mu5C)        == 0x5C, "CylinderVolume::mu5C");

} // namespace collision
} // namespace rw
