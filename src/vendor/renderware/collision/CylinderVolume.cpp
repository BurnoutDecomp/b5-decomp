#include "vendor/renderware/collision/CylinderVolume.hpp"

#include "vendor/renderware/collision/AABBox.hpp"       // AABBox + math::vpu::Vector3 -- moved OUT of
                                                        //   CylinderVolume.hpp on 2026-08-27 so game
                                                        //   TUs can include the header; see its banner
#include "rw/rwcore_structs.h"
#include "vendor/renderware/collision/GPInstance.hpp"   // GPInstance + g_aGPVolumeMethods

#include <cmath>     // sqrt, fabs
#include "types.hpp"   // uintptr_t (the console 32-bit mVolumeTag pointer image)
#include <cstring>   // memcpy (GetMaximumFeature edge copy)

// ===========================================================================
// rw::collision::CylinderVolume -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following this directory's
// precedent (TriangleVolume.cpp / GPCylinder.cpp), these are SEMANTIC
// reconstructions lowered to portable scalar float maths, preserving the store
// order, offsets and every side effect. The VMX reciprocal-sqrt idiom
// (vrsqrtefp + two Newton-Raphson refines, then x * rsqrt(x)) de-optimises to
// the exact sqrt it approximates; the vcmpeqfp/vsel zero-guard is kept.
//
// CreateGPInstance @ 0x82BAC7F8 IS HOMED HERE as of 2026-08-19 (wave Q6
// cluster C4). Its former park reason -- "un-homed GP VolumeMethods table
// unk_82F918F0[5] (runtime dispatch rodata)" -- was stale: that table is
// rw::collision::g_aGPVolumeMethods, defined in GPRegistration.cpp since
// 2026-08-18 and re-dumped from the image again this cluster
// (scratchpad/waveQ6/ida_vt2/out.json).
//
// The line kernel is homed in LineSegIntersect.cpp beside the other volumes'
// (2026-09-28, owner's list, lane L2 CAMCOLLIDE, stage (c); see the header):
//   LineSegIntersect     @ 0x82BAF688 --   8 insns, the tail-call dispatcher
//   FatLineSegIntersect  @ 0x82BAEB10 -- 733 insns
//   ThinLineSegIntersect @ 0x82BADCE0 -- 441 insns
// ===========================================================================

namespace rw
{
namespace collision
{

// --- Initialize's +0x40 stamp: NOT a .rdata constant -----------------------
// dword_8327EEF4 -- module-scope word stamped at +0x40, loaded via
// `lis/addi dword_8327EEE0 ; lwz +0x14`.
//
// CORRECTED 2026-08-18 (waveQ5 C1, headless IDA on a private .i64 copy). The
// old comment here called this an "un-recovered .rdata Initialize stamp", i.e.
// a compile-time constant whose value the export happened not to carry. It is
// neither: dword_8327EEE0 is the SEVEN-ENTRY RUNTIME Volume descriptor table
// (rw::collision::gVolumeVTable, defined in SDKs/EATech/rwcollision/volume.cpp
// and filled by rw::collision::Volume::InitializeVTable @ 0x82BB03A8), and
// +0x14 is slot 5 == VOLUMETYPECYLINDER. So the stamp is a RUNTIME READ of
// gVolumeVTable[5] -- the cylinder Volume descriptor POINTER -- exactly the
// value PrimitiveIntersect.cpp's GetVolumeVTable reads back from Volume+0x40.
// The image bytes at 0x8327EEE0..0x8327EEFC are all ZERO (they are .data, not
// .rdata) -- AGENTS gotcha 13: a zero there is "not written yet", not a value.
//
// HOST REPRESENTATION (2026-08-18, wave Q5 integration): the +0x40 slot holds
// the 4-byte VolumeType enum for the record's whole lifetime (an x64 pointer
// would overlap +0x44, and the 96-byte serialised record cannot grow); every
// reader recovers the console pointer as gVolumeVTable[enum]. So Initialize
// stamps VOLUMETYPECYLINDER (5, the index the console pointer sits at).
// Derivation: scratchpad/waveQ5/rwc3.owner.md section 7 / volume.cpp foot.
static const u32 KU_CYLINDER_VOLUME_TYPE = 5u;   // gVolumeVTable[5] = &unk_82F91894

// --- Initialize's default local-frame seed rows ----------------------------
// RECOVERED 2026-08-18 (waveQ5 C1, headless IDA on a private .i64 copy). These
// were three `extern const Vec4` declarations FLAGGED as "un-recovered axis
// seeds", i.e. three permanent link holes standing in for values that were in
// the image all along. They are the three IDENTITY BASIS ROWS of the SDK's
// I/J/K run, and the .rdata carries them verbatim:
//   0x82181500  w::math::vpu::detail::gIVector  3F800000 00000000 00000000 00000000
//   0x82181510  unk_82181510                    00000000 3F800000 00000000 00000000
//   0x82181520  unk_82181520                    00000000 00000000 3F800000 00000000
// (The IDA symbol is TRUNCATED to `w::math::vpu::detail::gIVector` -- AGENTS
// gotcha 6; the real name is rw::math::vpu::detail::gIVector.)
//
// Spelled as file-scope constants here, not as a new global, for the same
// reason GameSource/Director/Camera/Utils/CameraUtils.cpp:136 spells gIVector
// as a local KV_AXIS_X: the SDK's rw::math::vpu::detail basis run has no home
// TU anywhere in this tree, and inventing `rw::collision::g_vIVector` to
// satisfy three reads would be a second definition of a name that belongs to
// rw::math::vpu::detail (AGENTS gotcha 7). When that home lands, these three
// become references to it.
namespace
{
    const Vec4 KV_BASIS_X = { 1.0f, 0.0f, 0.0f, 0.0f };   // gIVector     @0x82181500
    const Vec4 KV_BASIS_Y = { 0.0f, 1.0f, 0.0f, 0.0f };   // unk_82181510 @0x82181510
    const Vec4 KV_BASIS_Z = { 0.0f, 0.0f, 1.0f, 0.0f };   // unk_82181520 @0x82181520
}

namespace
{
    // vspltisw(-1)/vslw sign-mask abs + the vmulfp/rsqrt fold: |1 - c^2| forced
    // to 0 by the vcmpeqfp/vsel guard, otherwise its true square root.
    inline f32 GuardedSqrt(f32 afValue)
    {
        return (afValue == 0.0f) ? 0.0f : afValue * (1.0f / std::sqrt(afValue));
    }

    // --- CreateGPInstance's transform prologue ------------------------------
    // vmaddfp vD,vA,vB,vC == vA*vC + vB in this family's asm. All FOUR VMX
    // lanes are computed: for an affine matrix the w column is 0, but the
    // console does not special-case it, so neither does this. (The same two
    // shapes CapsuleVolume.cpp uses -- each TU translates its OWN asm, which is
    // this directory's precedent, cf. TriangleVolume.cpp's TransformPoint /
    // TransformDirection pair.)

    // Rotate a direction row by the transform's 3x3, in the asm's accumulation
    // order: dir.x*row0, then += dir.y*row1, then += dir.z*row2.
    inline Vec4 RotateByTransform3x3(const Vec4& arDir, const Vec4* lapRows)
    {
        Vec4 lvOut;
        lvOut.x = arDir.x * lapRows[0].x;
        lvOut.y = arDir.x * lapRows[0].y;
        lvOut.z = arDir.x * lapRows[0].z;
        lvOut.w = arDir.x * lapRows[0].w;

        lvOut.x = arDir.y * lapRows[1].x + lvOut.x;
        lvOut.y = arDir.y * lapRows[1].y + lvOut.y;
        lvOut.z = arDir.y * lapRows[1].z + lvOut.z;
        lvOut.w = arDir.y * lapRows[1].w + lvOut.w;

        lvOut.x = arDir.z * lapRows[2].x + lvOut.x;
        lvOut.y = arDir.z * lapRows[2].y + lvOut.y;
        lvOut.z = arDir.z * lapRows[2].z + lvOut.z;
        lvOut.w = arDir.z * lapRows[2].w + lvOut.w;
        return lvOut;
    }

    // Transform a point by the full affine matrix. ACCUMULATION ORDER is the
    // asm's: row3 is folded in FIRST (`vmaddfp v0, v2, v7, v0` == point.x*row0
    // + row3), then point.y*row1, then point.z*row2.
    inline Vec4 TransformPointByTransform(const Vec4& arPoint, const Vec4* lapRows)
    {
        Vec4 lvOut;
        lvOut.x = arPoint.x * lapRows[0].x + lapRows[3].x;
        lvOut.y = arPoint.x * lapRows[0].y + lapRows[3].y;
        lvOut.z = arPoint.x * lapRows[0].z + lapRows[3].z;
        lvOut.w = arPoint.x * lapRows[0].w + lapRows[3].w;

        lvOut.x = arPoint.y * lapRows[1].x + lvOut.x;
        lvOut.y = arPoint.y * lapRows[1].y + lvOut.y;
        lvOut.z = arPoint.y * lapRows[1].z + lvOut.z;
        lvOut.w = arPoint.y * lapRows[1].w + lvOut.w;

        lvOut.x = arPoint.z * lapRows[2].x + lvOut.x;
        lvOut.y = arPoint.z * lapRows[2].y + lvOut.y;
        lvOut.z = arPoint.z * lapRows[2].z + lvOut.z;
        lvOut.w = arPoint.z * lapRows[2].w + lvOut.w;
        return lvOut;
    }
}

// ---------------------------------------------------------------------------
// CylinderVolume::Initialize @ 0x82BAD3F0
//
//   lwz r11, 0(r3) ; beq -> return 0          ; volume = *appVolume, NULL guard
//   stfs flt_82001CC0(0.0), 0x50              ; mfFatness = 0 (pre-clear)
//   stw  0, 0x54 / 0x58                        ; mu54 / mu58 = 0
//   stw  1, 0x5C                               ; muFlags = 1
//   stw  dword_8327EEF4, 0x40                  ; mInitWord = <module word>
//   stfs f3, 0x50 ; stfs f1, 0x48 ; stfs f2, 0x44   ; fatness / radius / half-h
//   stvx128 gIVector,     r11(0x00)            ; maFrame[0]
//   stvx128 0,            r11(0x30)            ; maFrame[3]
//   stvx128 unk_82181510, r11(0x10)            ; maFrame[1]
//   stvx128 unk_82181520, r11(0x20)            ; maFrame[2]
//   returns the volume pointer.
// ---------------------------------------------------------------------------
CylinderVolume* CylinderVolume::Initialize(const ::rw::Resource& arResource,
                                           f32 afRadius, f32 afHalfHeight, f32 afFatness)
{
    CylinderVolume* lpVolume = static_cast<CylinderVolume*>(arResource.m_baseResources[0]);
    if (!lpVolume) return 0;
    lpVolume->mfRadius = 0.0f;
    lpVolume->muGroupID = 0;
    lpVolume->muSurfaceID = 0;
    lpVolume->muFlags = 1;
    lpVolume->mpVTable = gVolumeVTable[VOLUMETYPECYLINDER];
    lpVolume->mfRadius = afFatness;
    lpVolume->mCylinderData.mfInnerRadius = afRadius;
    lpVolume->mCylinderData.mfHalfHeight = afHalfHeight;
    lpVolume->mTransform.xAxis = Vector3(1.0f, 0.0f, 0.0f);
    lpVolume->mTransform.wAxis = Vector3(0.0f, 0.0f, 0.0f);
    lpVolume->mTransform.yAxis = Vector3(0.0f, 1.0f, 0.0f);
    lpVolume->mTransform.zAxis = Vector3(0.0f, 0.0f, 1.0f);
    return lpVolume;
}

// ---------------------------------------------------------------------------
// CylinderVolume::GetBBoxDiag @ 0x82BAC7B8
//
//   lfs f0, 0x48(r4)  ; radius       lfs f13, 0x44(r4)  ; half-height
//   diag lane quad = { radius, radius, half-height, 0 }
//   vcfsx(vspltisw 2) = 2.0 ; diag *= 2.0 ; stvx128 -> r3 (return slot)
// X360 struct-return convention: r3 = the 16-byte return slot, r4 = this.
// ---------------------------------------------------------------------------
math::vpu::Vector3 CylinderVolume::GetBBoxDiag() const
{
    const Vector3 dimensions(mCylinderData.mfInnerRadius, mCylinderData.mfInnerRadius,
                             mCylinderData.mfHalfHeight);
    return Vector3(__vmulfp(dimensions.mV, __vcfsx(__vspltisw(2), 0)));
}

// ---------------------------------------------------------------------------
// CylinderVolume::CreateGPInstance @ 0x82BAC7F8  (98 instructions)
//
// r3 = this, r4 = &arInst, r5 = lpTransform (may be NULL). No null guard on r4
// -- the console dereferences it unconditionally.
//
//   cmplwi cr6, r5, 0 ; beq -> loc_82BAC89C (local path)
//   [transform path] all FOUR frame rows are transformed at once, the first
//     three as DIRECTIONS (3x3 only) and the fourth as a POINT (row3 folded in
//     first). The four results live in v0 = frame[0], v13 = frame[1],
//     v11 = frame[2], v12 = frame[3].
//   [local path]     the same four registers loaded straight from +0x00/+0x10/
//                    +0x20/+0x30 -- identical register assignment, which is why
//                    the store block below is shared.
//
//   then, in STORE ORDER:
//     stw     r11, 0x84(r4)      mVolumeTag         = (console 32-bit) this
//     stvx128 v12, r0,  r4       mPos       (+0x00) = frame[3]  (all 4 lanes)
//     stb     1,   0x8C(r4)      mNumFaceNormals    = 1
//     stw     5,   0x90(r4)      mVolumeType        = 5 (GPInstance::CYLINDER)
//     stb     1,   0x8D(r4)      mNumEdgeDirections = 1
//     stw     0,   0x88(r4)      mUserTag           = 0
//     stvx128 v13, r4, 0x20      mFaceNormals[1]    = frame[1]
//     stvx128 v0,  r4, 0x30      mFaceNormals[2]    = frame[0]
//     stvx128 v11, r4, 0x40      mEdgeDirections[0] = frame[2]
//     stvx128 v11, r4, 0x10      mFaceNormals[0]    = frame[2]   (the SAME row
//                                                     as the edge direction)
//     [+0x70 read-modify-write #1] mDimensions.x = mfHalfHeight (+0x44)
//     [+0x70 read-modify-write #2] mDimensions.y = mfRadius     (+0x48)
//     lwz     r10, 0x90(r4)      (the type is RE-READ from the instance, not
//                                 reused from the immediate in r3)
//     stw     r6,  0x94(r4)      mFlags   = this->muFlags (+0x5C)
//     stfs    f0,  0x80(r4)      mFatness = this->mfFatness (+0x50)
//     mMethods (+0xA4) = unk_82F918F0[type]   (4 words on console)
//     li r3, 1 ; blr
//
// TWO CONSOLE FACTS WORTH KEEPING, because both look like bugs and are not:
//
//  1. mNumFaceNormals is 1 while THREE face-normal rows are written. The
//     console stores the whole frame into +0x10/+0x20/+0x30 and then declares
//     only one of them live. GPCylinder's SAT callbacks
//     (g_aGPVolumeMethods[5], @0x82BAE430 / 0x82BAD7F8 / 0x82BAD938) read the
//     cylinder as "one face axis + one edge axis", so rows [1] and [2] are
//     carried but not enumerated. Reproduced exactly, not tidied.
//
//  2. The +0x70 stores are two separate READ-MODIFY-WRITE round trips through
//     a stack scratch quadword (`lvx128 +0x70 ; stvx128 stack ; stfs lane ;
//     lvx128 stack ; stvx128 +0x70`), lane x first and then lane y. Lanes z
//     and w of mDimensions are therefore left EXACTLY as the caller's buffer
//     had them -- NOT zeroed. Same treatment CapsuleVolume::CreateGPInstance
//     gets for its single lane-x write.
//
// CHECKLIST A (console literals vs host values): the console copies the
// VolumeMethods row with four lwz/stw pairs after `slwi r11, r10, 4` (its
// VolumeMethods is 4 x 4-byte pointers = 0x10 bytes). Those literals are
// CONSOLE-ONLY and stay in this comment: GPInstance::mMethods is
// pointer-widened on x64 (GPInstance.hpp), so the copy below is a whole-struct
// assignment and the index is a typed array index -- never a byte base +
// (type << 4).
// ---------------------------------------------------------------------------
RwBool CylinderVolume::CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const
{
    __vector4 lx = mTransform.xAxis.mV;
    __vector4 ly = mTransform.yAxis.mV;
    __vector4 lz = mTransform.zAxis.mV;
    __vector4 lw = mTransform.wAxis.mV;
    if (lpTransform)
    {
        const __vector4 tx = lpTransform->xAxis.mV;
        const __vector4 ty = lpTransform->yAxis.mV;
        const __vector4 tz = lpTransform->zAxis.mV;
        const __vector4 tw = lpTransform->wAxis.mV;
        __vector4 x = __vmulfp(__vspltw(lx, 0), tx);
        __vector4 y = __vmulfp(__vspltw(ly, 0), tx);
        __vector4 z = __vmulfp(__vspltw(lz, 0), tx);
        __vector4 w = __vmaddfp(__vspltw(lw, 0), tx, tw);
        x = __vmaddfp(__vspltw(lx, 1), ty, x);
        y = __vmaddfp(__vspltw(ly, 1), ty, y);
        z = __vmaddfp(__vspltw(lz, 1), ty, z);
        w = __vmaddfp(__vspltw(lw, 1), ty, w);
        lx = __vmaddfp(__vspltw(lx, 2), tz, x);
        ly = __vmaddfp(__vspltw(ly, 2), tz, y);
        lz = __vmaddfp(__vspltw(lz, 2), tz, z);
        lw = __vmaddfp(__vspltw(lw, 2), tz, w);
    }
    arInst.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mPos.mV = lw;
    arInst.mNumFaceNormals = 1;
    arInst.mVolumeType = GPInstance::CYLINDER;
    arInst.mNumEdgeDirections = 1;
    arInst.mUserTag = 0;
    arInst.mFaceNormals[1].mV = ly;
    arInst.mFaceNormals[2].mV = lx;
    arInst.mEdgeDirections[0].mV = lz;
    arInst.mFaceNormals[0].mV = lz;
    arInst.mDimensions.mV.x = mCylinderData.mfHalfHeight;
    arInst.mDimensions.mV.y = mCylinderData.mfInnerRadius;
    arInst.mFlags = muFlags;
    arInst.mFatness = mfRadius;
    arInst.mMethods = g_aGPVolumeMethods[GPInstance::CYLINDER];
    return 1;
}

// ---------------------------------------------------------------------------
// CylinderVolume::GetBBox @ 0x82BAD490
//
// Three paths (asm-attested):
//   1. lpTransform == NULL: local axis-aligned box, half-extent
//      { radius+fatness, radius+fatness, halfHeight+fatness }, centred at the
//      origin -> min = -extent, max = +extent.
//   2. lpTransform != NULL, !abTight: loose bounding-sphere half-extent
//      splat( sqrt( (fatness+halfHeight)^2 + (fatness+radius)^2 ) ) about the
//      transform translation (row 3).
//   3. lpTransform != NULL, abTight: tight oriented half-extent per world axis
//      i, with the cylinder axis = the transform's row-2 (lvx128 [r4+0x20]):
//        extent[i] = |axis[i]| * (fatness+halfHeight)
//                  + sqrt(|1 - axis[i]^2|) * (fatness+radius)
//      about the transform translation (row 3).
// All paths: min = centre - extent, max = centre + extent; returns 1.
// ---------------------------------------------------------------------------
static __vector4 CylinderSqrt(__vector4 value)
{
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 isZero = __vcmpeqfp(zero, value);
    __vector4 estimate = __vrsqrtefp(value);
    __vector4 square = __vmulfp(estimate, estimate);
    __vector4 scale = __vmulfp(estimate, half);
    __vector4 correction = __vnmsubfp(value, square, one);
    estimate = __vmaddfp(scale, correction, estimate);
    square = __vmulfp(estimate, estimate);
    scale = __vmulfp(estimate, half);
    correction = __vnmsubfp(value, square, one);
    estimate = __vmaddfp(scale, correction, estimate);
    return __vsel(__vmulfp(value, estimate), zero, isZero);
}

static __vector4 CylinderAxisExtent(__vector4 axis, __vector4 height, __vector4 radius)
{
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 sq = __vmulfp(axis, axis);
    const f32 square = sq.x;
    const f32 difference = 1.0f - square;
    const __vector4 absDifference = __vandc(__vspltw(__lvlx(&difference, 0), 0), sign);
    const __vector4 axial = __vmulfp(__vandc(axis, sign), height);
    return __vmaddfp(CylinderSqrt(absDifference), radius, axial);
}

RwBool CylinderVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const
{
    const f32 radial = mfRadius + mCylinderData.mfInnerRadius;
    const f32 axial = mfRadius + mCylinderData.mfHalfHeight;
    if (!lpTransform)
    {
        const Vector3 extent(radial, radial, axial);
        const __vector4 ones = __vspltisw(-1);
        const __vector4 sign = __vslw(ones, ones);
        arResult.mMax = extent;
        arResult.mMin.mV = __vxor(extent.mV, sign);
        return 1;
    }
    __vector4 extent;
    if (!abTight)
    {
        volatile f32 radiusSquare = radial * radial;
        const f32 distanceSquare = axial * axial + radiusSquare;
        const __vector4 distance = CylinderSqrt(__vspltw(__lvlx(&distanceSquare, 0), 0));
        const f32 d = distance.x;
        extent = Vector3(d, d, d).mV;
    }
    else
    {
        const __vector4 axis = lpTransform->zAxis.mV;
        const __vector4 height = __vspltw(__lvlx(&axial, 0), 0);
        const __vector4 radius = __vspltw(__lvlx(&radial, 0), 0);
        const __vector4 x = CylinderAxisExtent(__vspltw(axis, 0), height, radius);
        const __vector4 y = CylinderAxisExtent(__vspltw(axis, 1), height, radius);
        const __vector4 z = CylinderAxisExtent(__vspltw(axis, 2), height, radius);
        // The original builds this row from the spilled axis-x square; its W
        // lane survives the three component inserts.
        const __vector4 axisX = __vspltw(axis, 0);
        extent = __vrlimi(__vmulfp(axisX, axisX), x, 8, 0);
        extent = __vrlimi(extent, y, 4, 0);
        extent = __vrlimi(extent, z, 2, 0);
    }
    const __vector4 centre = lpTransform->wAxis.mV;
    arResult.mMin.mV = __vsubfp(centre, extent);
    arResult.mMax.mV = __vaddfp(centre, extent);
    return 1;
}

// ---------------------------------------------------------------------------
// CylinderVolume::GetMaximumFeature @ 0x82BAC988
//
//   axis   = maFrame[1] (+0x10)   centre = maFrame[3] (+0x30)
//   h      = mfHalfHeight (+0x44, splat)
//   P0     = centre + axis*h      (vmaddfp: axis*h + centre)
//   P1     = centre - axis*h      (vsubfp:  centre - axis*h)
//   FeatureEdge(edge, P0, P1) ; memcpy(feature+0x10, edge, 0x40) ; numedges = 1
// abCcw (r4) / arDir (r5) are dead in the asm.
// ---------------------------------------------------------------------------
RwBool CylinderVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vector3& /*arDir*/, Feature& arResult) const
{
    const __vector4 axis = mTransform.yAxis.mV;
    const __vector4 centre = mTransform.wAxis.mV;
    const f32 halfHeight = mCylinderData.mfHalfHeight;
    const __vector4 height = __vspltw(__lvlx(&halfHeight, 0), 0);
    const Vector3 plus(__vmaddfp(axis, height, centre));
    const Vector3 minus(__vsubfp(centre, __vmulfp(axis, height)));
    const FeatureEdge edge(plus, minus);
    memcpy(&arResult.edges[0], &edge, sizeof(edge));
    arResult.numedges = 1;
    return 1;
}

} // namespace collision
} // namespace rw

// ARTIST folds these genuine per-type methods to 82BAC980, 82B0F1B8 or 82AD5078.
namespace rw { namespace collision {
RwBool CylinderVolume::GetInterval(const Vector3&, Interval&) const { return 1; }
void CylinderVolume::Release() {}
} }
