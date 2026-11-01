#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "SDKs/EATech/rwcollision/volume_debug_access.h"
#include "vendor/renderware/collision/Feature.hpp"    // Vec4 / RwBool / FeatureEdge / Feature

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
//     rw::collision::CylinderVolume::CreateGPInstance    @ 0x82BAC7F8   [homed
//        2026-08-19, wave Q6 -- see the CORRECTION below]
//     rw::collision::CylinderVolume::GetBBox             @ 0x82BAD490
//     rw::collision::CylinderVolume::GetMaximumFeature   @ 0x82BAC988
//
// CORRECTION 2026-08-19 (wave Q6 cluster C4). CreateGPInstance used to sit in
// the BLOCKED list below with the reason "installs the un-homed GP-cylinder
// VolumeMethods table unk_82F918F0[5] ... that runtime-dispatch pointer table
// is un-recovered rodata". That reason went STALE IN THE HELPFUL DIRECTION
// (AGENTS gotcha 10): the table was recovered and HOMED on 2026-08-18 as
// rw::collision::g_aGPVolumeMethods (defined in GPRegistration.cpp, declared
// in GPInstance.hpp), and all six rows were re-dumped from the shipped image
// again this cluster -- scratchpad/waveQ6/ida_vt2/out.json, row [5] CYLINDER =
// { 0x82BAE430 GPCylinder::GetMaximumFeature, 0x82BAD7F8 GetInterval,
//   0x82BAD938 GetIntervals, 0x82AD5078 the ICF-folded empty `blr` }.
// Nothing about the body was ever blocked; only the table was, and it is not.
//
// LANDED 2026-09-28 (owner's list, lane L2 CAMCOLLIDE, stage (c)) -- the three
// that were BLOCKED here, bodied in LineSegIntersect.cpp beside the other
// volumes' line kernels, every lane checked against the ARTIST words run on
// emu64 (tests/run_l2_cylinder_line.py), and the descriptor slot bound in
// VolumeVTables.cpp:
//     rw::collision::CylinderVolume::LineSegIntersect     @ 0x82BAF688
//        -- eight instructions, a pure two-way TAIL-CALL dispatcher:
//             lfs   f0, 0x50(r3)          ; f0  = this->mfFatness
//             fadds f13, f0, f1           ; f13 = mfFatness + the caller's fatness
//             lfs   f0, flt_82001CC0      ; == 0.0f (word 0x00000000)
//             fcmpu cr6, f13, f0
//             bne   cr6, loc_82BAF6A4     ; total fatness != 0 (or NaN) -> the fat arm
//             b     ThinLineSegIntersect  ; 0x82BAF6A0, the fall-through arm
//           loc_82BAF6A4:
//             b     FatLineSegIntersect
//     rw::collision::CylinderVolume::FatLineSegIntersect  @ 0x82BAEB10
//        -- 733 instructions (0x82BAEB10..0x82BAF684): the cylinder grown by the
//           total fatness, its cap edges rounded by a torus (AALineClipper::
//           AALineClipper, rwcCylinderLineSegIntersect, rwcTorusLineSegIntersect).
//           FLAG (PC-platform): the torus root comes from SolveQuarticRoots, whose
//           five inlined VMX powf cascades are evaluated as powf (VmxPowF).
//     rw::collision::CylinderVolume::ThinLineSegIntersect @ 0x82BADCE0
//        -- 441 instructions (0x82BADCE0..0x82BAE3C4), over
//           rwcCylinderLineSegIntersect.
//
// NO DWARF / Feb-2007 source exists for this TU. The LAYOUT below is entirely
// X360-asm-attested (member OFFSETS are ground truth, pinned by the
// static_asserts; the member NAMES are inferred and documented). The 16-byte
// vector rows are carried as the shared scalar rw::collision::Vec4, the same
// vocabulary FeatureEdge / Feature / TriangleVolume trade in.
// ===========================================================================

// ---------------------------------------------------------------------------
// ⭐ 2026-08-27 (detached-part collision wave): `#include AABBox.hpp` REPLACED by the two
// forward declarations below, for exactly the reason CollisionVolume.hpp:89-96 already
// states about the SAME header -- AABBox.hpp drags the EATech `rw::math::vpu` vector
// vocabulary, which is a hard FORK with the vendor `rw::math::vpu::Vector3` that
// BrnCommonTypes.h defines. Until now that never mattered, because no GAME translation
// unit had ever included this header (grepped: only CylinderVolume.cpp and
// VolumeVTables.cpp did). BrnPhysicalWheel.cpp now does -- PhysicalWheel::AddToScene
// @0x8260C540 calls CylinderVolume::Initialize -- and co-including both vocabularies is a
// 100-error redefinition cascade, measured. A reference out-parameter and a
// declaration-only return type need no definition, so both are NAMED here instead and the
// two vendor TUs that actually CALL them include AABBox.hpp themselves.
//
// ⚠️ FLAGGED, NOT SMOOTHED: this means `GetBBoxDiag`'s return type resolves to the EATech
// `class Vector3` in the vendor TUs and to the vendor `struct Vector3` in a game TU, which
// mangle differently (V.. vs U..). That is SAFE ONLY BECAUSE no game TU calls GetBBoxDiag
// -- a declaration emits no symbol. If one ever does, it will be an LNK2019, not silent
// breakage. The underlying duplicate-vocabulary fork is the one CollisionVolume.hpp:57-65
// records as a separate outstanding job; this header does not resolve it, it stays out of
// its way.
// ---------------------------------------------------------------------------
namespace rw { struct Resource; }

namespace rw
{
namespace math
{
namespace vpu
{
    class Vector3;   // GetBBoxDiag's BY-VALUE return -- legal incomplete in a declaration
}
}
}

namespace rw
{
namespace collision
{

class AABBox;        // vendor/renderware/collision/AABBox.hpp -- GetBBox's reference out-param
                     // (same NAMED-not-included precedent as CollisionVolume.hpp:89-96)

struct VolumeLineSegIntersectResult;   // vendor/renderware/collision/LineSegIntersect.hpp (the line kernels)
struct GPInstance;   // vendor/renderware/collision/GPInstance.hpp (same
                     // forward-decl precedent as CapsuleVolume.hpp:56)

class CylinderVolume : public Volume
{
public:
    // Canonical default ctor (members left uninitialised until Initialize).
    CylinderVolume() {}

    RwBool GetInterval(const Vector3& arDir, Interval& arInterval) const;
    void Release();

    // @ 0x82BAC7F8 -- instance this cylinder into the GP ("generalised
    // primitive") narrow-phase image arInst, optionally transformed by the
    // 4-row affine matrix lpTransform (NULL = local space). This is the
    // collision-volume descriptor's createGPInstance slot (DWARF volume.h:1512,
    // console record word +0x14); the X360 register contract is the plain
    // __fastcall member one: r3 = this, r4 = &arInst, r5 = lpTransform. There
    // is NO null guard on arInst -- the console dereferences r4 unconditionally.
    // The console always returns 1 (li r3, 1) and every caller discards it.
    //
    // DWARF cylinder.h:190 spells the transform
    // `const rw::math::vpu::Matrix44Affine*`; this directory carries the four
    // 16-byte rows as `const Vec4*` (the Matrix44Affine IS four Vec4 rows --
    // MEASURED, scratchpad/waveQ5/probe_vtbind/measure_vocab*.cpp), which is
    // the spelling CapsuleVolume::CreateGPInstance and the descriptor adapter
    // in VolumeVTables.cpp already use.
    RwBool CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const;

    // @ 0x82BAD3F0 -- stamp the cylinder's default local frame + the three
    // dimensions into the volume referenced by *appVolume and return it (NULL
    // when the slot is empty). X360 __fastcall: r3 = the slot holding the volume
    // pointer (`lwz r11, 0(r3)`); f1 = radius, f2 = half-height, f3 = fatness.
    static CylinderVolume* Initialize(const ::rw::Resource& arResource,
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
    RwBool GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const;

    // @ 0x82BAC988 -- build the cylinder's maximum contact feature: the central
    // axis segment (centre +/- axis * half-height) as a single FeatureEdge in
    // arResult.edges[0], numedges = 1. abCcw / arDir are accepted but unused
    // (r4 / r5 are dead in the asm; the cylinder axis is always its maximum
    // feature here).
    RwBool GetMaximumFeature(RwBool abCcw, const Vector3& arDir, Feature& arResult) const;

    // @ 0x82BADCE0 (441 insns) -- the ZERO-fatness arm of the cylinder's line kernel: the segment arPt1 -> arPt2
    // against the frame's z cylinder |z| <= mfHalfHeight, radius mfRadius, in its frame (composed with lpTransform
    // when given); fills arResult on a hit (1). LANDED 2026-09-28 (owner's list, lane L2, stage (c)); body in
    // LineSegIntersect.cpp. The dispatcher LineSegIntersect @0x82BAF688 below sends it every call whose
    // mfFatness + afFatness is exactly 0. The same argument list as the other volumes' LineSegIntersect
    // (r3..r7, f1); the thin arm never reads afFatness.
    RwBool ThinLineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                                VolumeLineSegIntersectResult& arResult) const;

    // @ 0x82BAEB10 (733 insns) -- the FATTENED arm: the cylinder grown by mfFatness + afFatness, its cap edges
    // rounded by a torus (rwcTorusLineSegIntersect over an AALineClipper box). LANDED 2026-09-28 (lane L2,
    // stage (c)); body in LineSegIntersect.cpp.
    RwBool FatLineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                               VolumeLineSegIntersectResult& arResult, f32 afFatness) const;

    // @ 0x82BAF688 (8 insns) -- the descriptor's lineSegIntersect slot: mfFatness + afFatness == 0 tail-calls
    // ThinLineSegIntersect, anything else (a NaN included) FatLineSegIntersect. LANDED 2026-09-28; bound in
    // VolumeVTables.cpp.
    RwBool LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2, const Matrix44Affine* lpTransform,
                            VolumeLineSegIntersectResult& arResult, f32 afFatness) const;


};

CGS_STATIC_ASSERT((sizeof(CylinderVolume) == 0x60), "Native CylinderVolume size");

} // namespace collision
} // namespace rw
