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
// BLOCKED (not reconstructed here -- see CylinderVolume.cpp for the precise
// reasons; declarations omitted per the TriangleVolume precedent):
//     rw::collision::CylinderVolume::LineSegIntersect     @ 0x82BAF688
//        -- MEASURED 2026-08-19 (wave Q6 targeted headless idat on a PRIVATE
//           .i64 copy; this address had NO per-address export JSON, which is
//           why the wave-Q5 record could only call it "unsized"):
//           EIGHT instructions, and the whole body is recovered --
//             lfs   f0, 0x50(r3)          ; f0  = this->mfFatness
//             fadds f13, f0, f1           ; f13 = mfFatness + the caller's fatness
//             lfs   f0, flt_82001CC0      ; == 0.0f (word 0x00000000, re-dumped)
//             fcmpu cr6, f13, f0
//             bne   cr6, loc_82BAF6A4     ; total fatness != 0 -> the fat arm
//             b     ThinLineSegIntersect  ; 0x82BAF6A0, the fall-through arm
//           loc_82BAF6A4:
//             b     FatLineSegIntersect
//           i.e. a pure two-way TAIL-CALL dispatcher: total fatness exactly
//           zero picks the thin kernel, anything else the fat one.
//           IT IS STILL BLOCKED, and the blocker is now named exactly: both
//           tail-call targets are unreconstructed and have NO body anywhere in
//           the tree, so landing the eight instructions would plant two
//           guaranteed LNK2019s in an already-mounted TU (AGENTS gotcha 12 --
//           `cl /c` cannot see that). Land the two kernels first, then this.
//     rw::collision::CylinderVolume::FatLineSegIntersect  @ 0x82BAEB10
//        -- MEASURED: 733 instructions (0x82BAEB10..0x82BAF684). Calls
//           AALineClipper::AALineClipper, rwcCylinderLineSegIntersect,
//           rwcTorusLineSegIntersect, with __savevmx_117/__savefpr_25 hand
//           register allocation.
//     rw::collision::CylinderVolume::ThinLineSegIntersect @ 0x82BADCE0
//        -- MEASURED: 441 instructions (0x82BADCE0..0x82BAE3C4). Calls
//           rwcCylinderLineSegIntersect, with __savevmx_124 hand register
//           allocation.
//        (The two counts above replace this banner's earlier "~500-instruction"
//        estimate -- they are now dumped, not guessed. A faithful de-optimised
//        reconstruction of their per-lane geometry still cannot be grounded
//        with confidence, so they stay BLOCKED per the no-fabrication rule.)
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

struct GPInstance;   // vendor/renderware/collision/GPInstance.hpp (same
                     // forward-decl precedent as CapsuleVolume.hpp:56)

class CylinderVolume
{
public:
    // Canonical default ctor (members left uninitialised until Initialize).
    CylinderVolume() {}

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
    RwBool CreateGPInstance(GPInstance& arInst, const Vec4* lpTransform) const;

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
    // maFrame[0]/[1]/[2] = the three identity basis rows (gIVector @0x82181500,
    // unk_82181510, unk_82181520 -- all three RECOVERED, see the .cpp) and
    // maFrame[3] = 0. GetMaximumFeature reads maFrame[1] as the cylinder axis
    // and maFrame[3] as the centre; CreateGPInstance transforms all four rows
    // ([0..2] as directions, [3] as a point) and lays them into the GP image as
    // mFaceNormals[2]/[1]/[0] = frame[0]/[1]/[2], mEdgeDirections[0] = frame[2],
    // mPos = frame[3].
    Vec4 maFrame[4];    // +0x00 / +0x10 / +0x20 / +0x30
    u32  mInitWord;     // +0x40  Initialize's dword_8327EEF4 (see .cpp)
    f32  mfHalfHeight;  // +0x44  cylinder half-height (axial extent); copied to
                        //         GPInstance::mDimensions.x (+0x70)
    f32  mfRadius;      // +0x48  cylinder radius (radial extent); copied to
                        //         GPInstance::mDimensions.y (+0x74)
    u32  mu4C;          // +0x4C  (unread by this TU; keeps +0x50 aligned)
    f32  mfFatness;     // +0x50  surface fatness (0 by default); copied to
                        //         GPInstance::mFatness (+0x80)
    u32  mu54;          // +0x54  (Initialize: 0)
    u32  mu58;          // +0x58  (Initialize: 0)
    // RENAMED 2026-08-19 (wave Q6): was `mu5C`, "state/type word". It has a
    // measured role now -- CreateGPInstance @0x82BAC7F8 copies it VERBATIM into
    // GPInstance::mFlags (`lwz r6, 0x5C(r11) ; stw r6, 0x94(r4)`), exactly the
    // role CapsuleVolume::muFlags (+0x5C) and TriangleVolume::mFlags (+0x5C)
    // play. Nothing outside this class ever named it (grepped).
    u32  muFlags;       // +0x5C  volume flag word (Initialize: 1); copied
                        //         verbatim into GPInstance::mFlags (+0x94)
};

static_assert(offsetof(CylinderVolume, maFrame)     == 0x00, "CylinderVolume::maFrame");
static_assert(offsetof(CylinderVolume, mInitWord)   == 0x40, "CylinderVolume::mInitWord");
static_assert(offsetof(CylinderVolume, mfHalfHeight)== 0x44, "CylinderVolume::mfHalfHeight");
static_assert(offsetof(CylinderVolume, mfRadius)    == 0x48, "CylinderVolume::mfRadius");
static_assert(offsetof(CylinderVolume, mfFatness)   == 0x50, "CylinderVolume::mfFatness");
static_assert(offsetof(CylinderVolume, muFlags)     == 0x5C, "CylinderVolume::muFlags");

} // namespace collision
} // namespace rw
