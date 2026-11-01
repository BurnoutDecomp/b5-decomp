#include "vendor/renderware/collision/CollisionVolume.hpp"

#include "vendor/renderware/collision/AABBox.hpp"   // AABBox (the GetBBox out-parameter)
#include "vendor/renderware/collision/Feature.hpp"  // Feature (the GetMaximumFeature out-parameter)
#include "vendor/renderware/collision/GPInstance.hpp" // GPInstance + g_aGPVolumeMethods
#include "rw/rwcore_structs.h"                      // rw::Resource (complete, for Initialize)

#include <cmath>   // fabs
#include <new>     // placement new (Initialize constructs into the caller's block)

// ===========================================================================
// rw::collision::BoxVolume and rw::collision::SphereVolume -- reconstructed from
// BURNOUT_X360_ARTIST.XEX, 2026-08-18 (wave Q5). Declarations + the full layout
// derivation live in CollisionVolume.hpp; this file carries the bodies.
//
//   rw::collision::BoxVolume::BoxVolume        @ 0x82BAA0F0   [homed here]
//   rw::collision::BoxVolume::Initialize       @ 0x82BAA188   [homed here]
//   rw::collision::BoxVolume::GetBBox          @ 0x82BA9FC8   [homed here]  (74 insns)
//   rw::collision::BoxVolume::GetBBoxDiag      @ 0x82BA8890   [homed here]
//   rw::collision::SphereVolume::Initialize    @ 0x82BA84E8   [homed here]
//   rw::collision::SphereVolume::GetBBox       @ 0x82BA8020   [homed here]
//   rw::collision::SphereVolume::GetBBoxDiag   @ 0x82BA8580   [homed here]
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following this directory's
// precedent (CapsuleVolume.cpp / CylinderVolume.cpp / TriangleVolume.cpp) these are
// SEMANTIC reconstructions lowered to portable scalar float maths, preserving the store
// order, the offsets and every side effect. Two idioms recur and are named once here:
//   * `vspltisw v0,-1 ; vslw v5,v0,v0 ; vandc vX,vX,v5` is the 0x80000000 sign-bit
//     clear, i.e. std::fabs per lane.
//   * `vmaddfp vD,vA,vB,vC` == vA*vC + vB (the operand rule attested across this
//     rw::collision family and restated at every site in CapsuleVolume.cpp).
// The console does the arithmetic on all FOUR lanes and stores 16-byte rows. Whether the
// W lane matters depends on the CONSUMER, and the two cases are treated differently on
// purpose:
//   * GetBBox / GetBBoxDiag fold into an AABBox / a diagonal whose lanes are x/y/z, so the
//     four GetBBox* bodies carry the three geometric lanes, exactly as their four siblings
//     in this directory do. (See the W-lane note at BoxVolume::GetBBox's `Set` calls: the
//     AABBox out-parameter's W lanes are left as the caller had them. Measured, reported.)
//   * CreateGPInstance STORES its transformed rows whole -- `stvx128`, sixteen bytes, into
//     GPInstance::mPos / mFaceNormals / mEdgeDirections -- so the W lane is a real stored
//     value there and the two four-lane helpers below (RotateRow4 / TransformPoint4) are
//     what those two bodies use. Dropping it would be a silent divergence.
//
// ---------------------------------------------------------------------------------------
// Four more bodies, so that the SPHERE and BOX descriptor records in VolumeVTables.cpp
// can bind their slots:
//
//   rw::collision::SphereVolume::GetMaximumFeature @ 0x82BA81B0   (9 insns)
//   rw::collision::BoxVolume::GetMaximumFeature    @ 0x82BA87F0   (9 insns)
//   rw::collision::SphereVolume::CreateGPInstance  @ 0x82BA8100   (~40 insns)
//   rw::collision::BoxVolume::CreateGPInstance     @ 0x82BA92E8   (~85 insns)
//
// The two LineSegIntersect bodies (and rwcPlaneLineSegIntersect) are in LineSegIntersect.cpp,
// beside the rwc* kernels they call, NOT here: this TU is compiled on its own by other tests
// (run_fxdirector2_scene_query links it for SphereVolume::Initialize) and must not gain a link
// dependency on the line-test TU. Their consumer is rw::collision::VolumeLineQuery::
// GetIntersections, which the director camera's line tests reach.
//   rw::collision::SphereVolume::LineSegIntersect  @ 0x82BA82C8
//   rw::collision::BoxVolume::LineSegIntersect     @ 0x82BA9478
// ===========================================================================

namespace rw
{
namespace collision
{

namespace
{
    // --- the three identity basis rows the two constructors seed -------------------
    // Same three .rdata rows CylinderVolume.cpp:74-77 already spells as file-scope
    // constants, and for the same reason (AGENTS gotcha 7): the SDK's
    // rw::math::vpu::detail basis run has no home TU anywhere in this tree, so inventing
    // a shared `rw::collision::g_vIVector` would be a second definition of a name that
    // belongs to rw::math::vpu::detail. When that home lands these become references to
    // it. Values dumped from the shipped image (wave Q5 C1, headless IDA on a private
    // .i64 copy):
    //   0x82181500  w::math::vpu::detail::gIVector  3F800000 00000000 00000000 00000000
    //   0x82181510  unk_82181510                    00000000 3F800000 00000000 00000000
    //   0x82181520  unk_82181520                    00000000 00000000 3F800000 00000000
    // (The IDA symbol is TRUNCATED to `w::math::vpu::detail::gIVector` -- gotcha 6.)
    const Vec4 KV_BASIS_X = { 1.0f, 0.0f, 0.0f, 0.0f };   // gIVector     @0x82181500
    const Vec4 KV_BASIS_Y = { 0.0f, 1.0f, 0.0f, 0.0f };   // unk_82181510 @0x82181510
    const Vec4 KV_BASIS_Z = { 0.0f, 0.0f, 1.0f, 0.0f };   // unk_82181520 @0x82181520
    const Vec4 KV_ZERO    = { 0.0f, 0.0f, 0.0f, 0.0f };   // vspltisw v0, 0

    // flt_82001CC0 == 0.0f -- the "fatness" pre-clear both constructors store at +0x50.
    // Same .rdata word CylinderVolume.cpp and CapsuleVolume.cpp already identify as 0.0f.
    const f32 KF_ZERO_FATNESS = 0.0f;

    // Seed the shared rw::collision::Volume part of a freshly-constructed primitive.
    // This is the base-class constructor the X360 folded into both bodies: the SAME
    // seven stores appear, in the SAME order, at the head of BoxVolume::BoxVolume
    // @0x82BAA0F0 and SphereVolume::Initialize @0x82BA84E8 -- an inlined
    // `Volume::Volume(VolumeType)` (DWARF volume.h:1388). De-inlined here per AGENTS
    // ("inlining reversal"), NOT invented.
    //
    //   stfs flt_82001CC0(0.0f), 0x50(vol)     ; radius / fatness
    //   stw  0,             0x54(vol)          ; groupID
    //   stw  0,             0x58(vol)          ; surfaceID
    //   stw  1,             0x5C(vol)          ; m_flags = VOLUMEFLAG_ISENABLED
    //   stw  gVolumeVTable[type], 0x40(vol)    ; the per-TYPE descriptor  [see the FLAG]
    //   stvx gIVector,      0x00(vol)          ; transform.xAxis
    //   stvx 0,             0x30(vol)          ; transform.wAxis
    //   stvx unk_82181510,  0x10(vol)          ; transform.yAxis
    //   stvx unk_82181520,  0x20(vol)          ; transform.zAxis
    //
    void ConstructVolume(Volume& arVolume, VolumeType aeType)
    {
        arVolume.mfRadius    = KF_ZERO_FATNESS;
        arVolume.muGroupID   = 0;
        arVolume.muSurfaceID = 0;
        arVolume.muFlags     = KU_VOLUMEFLAG_ISENABLED;

        arVolume.mpVTable = gVolumeVTable[aeType];
        arVolume.mTransform.xAxis = Vector3(1.0f, 0.0f, 0.0f);
        arVolume.mTransform.wAxis = Vector3(0.0f, 0.0f, 0.0f);
        arVolume.mTransform.yAxis = Vector3(0.0f, 1.0f, 0.0f);
        arVolume.mTransform.zAxis = Vector3(0.0f, 0.0f, 1.0f);
    }

    // Rotate a local basis row by the transform's 3x3 (the three vmulfp/vmaddfp chains
    // `m.x*T0 + m.y*T1 + m.z*T2`). Identical shape to CapsuleVolume::GetBBox's axis
    // rotation.
    void RotateRow(const Vec4& arLocalRow, const Vec4* lpTransform,
                   f32& arOutX, f32& arOutY, f32& arOutZ)
    {
        const Vec4& lvRow0 = lpTransform[0];
        const Vec4& lvRow1 = lpTransform[1];
        const Vec4& lvRow2 = lpTransform[2];

        arOutX = arLocalRow.x * lvRow0.x + arLocalRow.y * lvRow1.x + arLocalRow.z * lvRow2.x;
        arOutY = arLocalRow.x * lvRow0.y + arLocalRow.y * lvRow1.y + arLocalRow.z * lvRow2.y;
        arOutZ = arLocalRow.x * lvRow0.z + arLocalRow.y * lvRow1.z + arLocalRow.z * lvRow2.z;
    }

    // Transform a local translation row as a POINT (the same chain plus the transform's
    // own translation row -- in the asm the accumulation STARTS from T3:
    // `vmaddfp v0, row3.x, T3, T0` at 0x82BAA028 seeds `row3.x*T0 + T3`).
    void TransformPoint(const Vec4& arLocalRow, const Vec4* lpTransform,
                        f32& arOutX, f32& arOutY, f32& arOutZ)
    {
        const Vec4& lvRow3 = lpTransform[3];

        RotateRow(arLocalRow, lpTransform, arOutX, arOutY, arOutZ);

        arOutX += lvRow3.x;
        arOutY += lvRow3.y;
        arOutZ += lvRow3.z;
    }

    // --- the FOUR-LANE forms, for CreateGPInstance -----------------------------------
    // The two helpers above deliberately carry only x/y/z: their consumers (GetBBox /
    // GetBBoxDiag) fold into an AABBox, whose lanes are x/y/z. CreateGPInstance is
    // different -- it STORES the transformed rows whole (`stvx128`, sixteen bytes) into
    // GPInstance::mPos / mFaceNormals / mEdgeDirections, so the W lane the console's
    // vmaddfp chain computes is a real stored value and dropping it would be a silent
    // divergence. These two are the same accumulation, all four lanes, and are spelled
    // exactly like the sibling CapsuleVolume.cpp:78/:101 pair.
    Vec4 RotateRow4(const Vec4& arLocalRow, const Vec4* lpTransform)
    {
        Vec4 lvOut;
        lvOut.x = arLocalRow.x * lpTransform[0].x;
        lvOut.y = arLocalRow.x * lpTransform[0].y;
        lvOut.z = arLocalRow.x * lpTransform[0].z;
        lvOut.w = arLocalRow.x * lpTransform[0].w;

        lvOut.x = arLocalRow.y * lpTransform[1].x + lvOut.x;
        lvOut.y = arLocalRow.y * lpTransform[1].y + lvOut.y;
        lvOut.z = arLocalRow.y * lpTransform[1].z + lvOut.z;
        lvOut.w = arLocalRow.y * lpTransform[1].w + lvOut.w;

        lvOut.x = arLocalRow.z * lpTransform[2].x + lvOut.x;
        lvOut.y = arLocalRow.z * lpTransform[2].y + lvOut.y;
        lvOut.z = arLocalRow.z * lpTransform[2].z + lvOut.z;
        lvOut.w = arLocalRow.z * lpTransform[2].w + lvOut.w;
        return lvOut;
    }

    // Accumulation order is the asm's: the transform's own row 3 is folded in FIRST
    // (`vmaddfp v0, row3.x, T3, T0` seeds row3.x*T0 + T3), then y*T1, then z*T2.
    Vec4 TransformPoint4(const Vec4& arLocalRow, const Vec4* lpTransform)
    {
        Vec4 lvOut;
        lvOut.x = arLocalRow.x * lpTransform[0].x + lpTransform[3].x;
        lvOut.y = arLocalRow.x * lpTransform[0].y + lpTransform[3].y;
        lvOut.z = arLocalRow.x * lpTransform[0].z + lpTransform[3].z;
        lvOut.w = arLocalRow.x * lpTransform[0].w + lpTransform[3].w;

        lvOut.x = arLocalRow.y * lpTransform[1].x + lvOut.x;
        lvOut.y = arLocalRow.y * lpTransform[1].y + lvOut.y;
        lvOut.z = arLocalRow.y * lpTransform[1].z + lvOut.z;
        lvOut.w = arLocalRow.y * lpTransform[1].w + lvOut.w;

        lvOut.x = arLocalRow.z * lpTransform[2].x + lvOut.x;
        lvOut.y = arLocalRow.z * lpTransform[2].y + lvOut.y;
        lvOut.z = arLocalRow.z * lpTransform[2].z + lvOut.z;
        lvOut.w = arLocalRow.z * lpTransform[2].w + lvOut.w;
        return lvOut;
    }
}

// ---------------------------------------------------------------------------
// BoxVolume::BoxVolume @ 0x82BAA0F0 -- DWARF box.h:57.
//
//   lis/addi dword_8327EEE0 ; lwz r11, +0x10   ; gVolumeVTable[4] (BOX)
//   stvx128 v1, r1, 0x20                       ; spill the by-value Vector3 argument
//   lfs f0, arg_20 / f13, arg_24 / f12, arg_28 ; ...and re-read it as three scalars
//   <the nine inlined Volume::Volume stores -- see ConstructVolume>
//   stfs f0,  0x44(r3)                         ; boxData.hx
//   stfs f13, 0x48(r3)                         ; boxData.hy
//   stfs f12, 0x4C(r3)                         ; boxData.hz
//   blr                                        ; returns `this` in r3 (the ctor ABI)
//
// The half-extent stores come AFTER the transform/flag block in the asm; the order is
// preserved because ConstructVolume also writes +0x50 (the fatness) and the two blocks
// must not be interleaved differently from the console if anyone ever diffs them.
// ---------------------------------------------------------------------------
BoxVolume::BoxVolume(Vector3 arHalfDimensions)
{
    ConstructVolume(*this, VOLUMETYPEBOX);

    mBoxData.mfHx = arHalfDimensions.mV.x;
    mBoxData.mfHy = arHalfDimensions.mV.y;
    mBoxData.mfHz = arHalfDimensions.mV.z;
}

// ---------------------------------------------------------------------------
// BoxVolume::Initialize @ 0x82BAA188 -- DWARF box.h:109. Five instructions:
//
//   0x82BAA188  lwz    r3, 0(r3)              ; resource.m_baseResources[0]
//   0x82BAA18C  cmplwi r3, 0
//   0x82BAA190  beq    loc_82BAA198
//   0x82BAA194  b      BoxVolume::BoxVolume   ; TAIL call -- r3 = the block, v1 = dims
//   0x82BAA198  li     r3, 0
//   0x82BAA19C  blr
//
// r3 is the RESOURCE, not a `this` (word 0 of a BoxVolume is transform.xAxis.x, a
// float): the function is static. The tail call is a constructor invocation on raw
// memory, i.e. a placement new, and the ctor's r3 return is what Initialize returns.
// ---------------------------------------------------------------------------
BoxVolume* BoxVolume::Initialize(const ::rw::Resource& arResource,
                                 Vector3               arHalfDimensions)
{
    void* lpBlock = arResource.m_baseResources[0];
    if (lpBlock == 0)
    {
        return 0;
    }

    return new (lpBlock) BoxVolume(arHalfDimensions);
}

// ---------------------------------------------------------------------------
// BoxVolume::GetBBox @ 0x82BA9FC8 (74 insns) -- DWARF box.h:157.
//
//   cmplwi cr6, r4, 0 ; beq -> the NULL-transform arm
//   [transform arm]  compose this->transform * (*lpTransform) into four rows:
//        v12 = row0, v11 = row1, v10 = row2, v13 = row3   (0x82BA9FD4..0x82BAA05C)
//   [NULL arm]       load this->transform's four rows into the same registers
//                                                          (0x82BAA064..0x82BAA078)
//   lvlx/vspltw +0x48 -> hy, +0x44 -> hx, +0x4C -> hz, +0x50 -> radius
//   vandc row0/row1/row2 with the sign mask                ; |row|
//   v11 = |row1| * hy
//   v12 = |row0| * hx + v11
//   v0  = |row2| * hz + v12
//   v0  = v0 + radius
//   stvx128 (row3 - v0), 0x00(r6)  ; arResult.mMin
//   stvx128 (row3 + v0), 0x10(r6)  ; arResult.mMax
//   li r3, 1 ; blr
//
// abTight (r5) is NEVER READ -- there is no tight/loose split for a box.
// The returned RwBool is a constant 1; ActiveRaceCar::AddToScene @0x822EB95C, the only
// direct caller in the game, discards it.
// ---------------------------------------------------------------------------
RwBool BoxVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
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
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 hx = __vspltw(__lvlx(&mBoxData.mfHx, 0), 0);
    const __vector4 hy = __vspltw(__lvlx(&mBoxData.mfHy, 0), 0);
    const __vector4 hz = __vspltw(__lvlx(&mBoxData.mfHz, 0), 0);
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    __vector4 extent = __vmulfp(__vandc(ly, sign), hy);
    extent = __vmaddfp(__vandc(lx, sign), hx, extent);
    extent = __vmaddfp(__vandc(lz, sign), hz, extent);
    extent = __vaddfp(extent, radius);
    arResult.mMin.mV = __vsubfp(lw, extent);
    arResult.mMax.mV = __vaddfp(lw, extent);
    return 1;
}

// ---------------------------------------------------------------------------
// BoxVolume::GetBBoxDiag @ 0x82BA8890 -- DWARF box.h:160. Struct-return ABI:
// r3 = the hidden 16-byte return slot, r4 = this.
//
//   vspltisw v12, 2 ; vcfsx v12, v12, 0     ; the literal 2.0f
//   lvx128 +0x00 / +0x10 / +0x20            ; the LOCAL transform rows (no argument)
//   lvlx/vspltw +0x48 -> hy, +0x44 -> hx, +0x4C -> hz, +0x50 -> radius
//   vandc rows with the sign mask ; |row|
//   v11 = |row1| * hy
//   v13 = |row0| * hx + v11
//   v0  = |row2| * hz + v13
//   v0  = (v0 + radius) * 2.0f
//   stvx128 v0, r3
// ---------------------------------------------------------------------------
Vector3 BoxVolume::GetBBoxDiag() const
{
    const __vector4 lOnes = __vspltisw(-1);
    const __vector4 lSign = __vslw(lOnes, lOnes);
    const __vector4 lX = __vandc(mTransform.xAxis.mV, lSign);
    const __vector4 lY = __vandc(mTransform.yAxis.mV, lSign);
    const __vector4 lZ = __vandc(mTransform.zAxis.mV, lSign);
    const __vector4 lHx = __vspltw(__lvlx(&mBoxData.mfHx, 0), 0);
    const __vector4 lHy = __vspltw(__lvlx(&mBoxData.mfHy, 0), 0);
    const __vector4 lHz = __vspltw(__lvlx(&mBoxData.mfHz, 0), 0);
    const __vector4 lRadius = __vspltw(__lvlx(&mfRadius, 0), 0);
    __vector4 lExtent = __vmulfp(lY, lHy);
    lExtent = __vmaddfp(lX, lHx, lExtent);
    lExtent = __vmaddfp(lZ, lHz, lExtent);
    lExtent = __vaddfp(lExtent, lRadius);
    return Vector3(__vmulfp(lExtent, __vcfsx(__vspltisw(2), 0)));
}

// ---------------------------------------------------------------------------
// SphereVolume::Initialize @ 0x82BA84E8 -- DWARF sphere.h:12.
//
//   lwz r11, 0(r3) ; beq -> return 0        ; resource.m_baseResources[0]
//   <the nine inlined Volume::Volume stores, with gVolumeVTable[1] (SPHERE)>
//   stfs f1, 0x50(r11)                      ; radius -- OVERWRITES the 0.0f pre-clear
//   mr  r3, r11 ; blr
//
// The X360 folded the sphere's constructor into Initialize (there is no separate
// SphereVolume::SphereVolume symbol), which is why the base seed and the radius store
// both live here. The type union at +0x44..+0x4F is DELIBERATELY not written: a sphere's
// SphereSpecificData is the SDK's `void* nothing` (DWARF volumedata.h:80) and its size
// lives in Volume::radius. The two 0x50 stores are both reproduced -- the pre-clear is a
// real side effect of the inlined base constructor.
// ---------------------------------------------------------------------------
SphereVolume* SphereVolume::Initialize(const ::rw::Resource& arResource, f32 afRadius)
{
    void* lpBlock = arResource.m_baseResources[0];
    if (lpBlock == 0)
    {
        return 0;
    }

    SphereVolume* lpVolume = static_cast<SphereVolume*>(lpBlock);

    ConstructVolume(*lpVolume, VOLUMETYPESPHERE);
    lpVolume->mfRadius = afRadius;

    return lpVolume;
}

// ---------------------------------------------------------------------------
// SphereVolume::GetBBox @ 0x82BA8020 -- the centre +/- the radius.
//
//   lvx128 v0, r3, 0x30                     ; this->transform.wAxis == the centre
//   cmplwi cr6, r4, 0 ; beq -> skip the transform
//   vmaddfp chains                          ; transform the centre as a POINT
//   lvlx/vspltw +0x50 -> radius
//   stvx128 (centre - radius), 0x00(r6) ; stvx128 (centre + radius), 0x10(r6)
//   li r3, 1 ; blr
//
// Only ONE row of the volume's own transform is read (+0x30) -- a sphere has no
// orientation. abTight (r5) is never read here either.
// ---------------------------------------------------------------------------
RwBool SphereVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 centre = mTransform.wAxis.mV;
    if (lpTransform)
    {
        __vector4 result = __vmaddfp(__vspltw(centre, 0), lpTransform->xAxis.mV, lpTransform->wAxis.mV);
        result = __vmaddfp(__vspltw(centre, 1), lpTransform->yAxis.mV, result);
        centre = __vmaddfp(__vspltw(centre, 2), lpTransform->zAxis.mV, result);
    }
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    arResult.mMin.mV = __vsubfp(centre, radius);
    arResult.mMax.mV = __vaddfp(centre, radius);
    return 1;
}

// ---------------------------------------------------------------------------
// SphereVolume::GetBBoxDiag @ 0x82BA8580 -- 2 * radius on every axis.
//
//   vspltisw v0, 2 ; vcfsx v0, v0, 0        ; 2.0f
//   lvlx/vspltw +0x50 -> radius             ; a SPLAT, so every lane is 2*radius
//   vperm / vrlimi128                       ; a lane shuffle of an all-lanes-equal
//                                           ; vector -- inert (the compiler's generic
//                                           ; Vector3(s,s,s) construction)
//   stvx128 v13, r0, r3
// ---------------------------------------------------------------------------
Vector3 SphereVolume::GetBBoxDiag() const
{
    const __vector4 lRadius = __vspltw(__lvlx(&mfRadius, 0), 0);
    const __vector4 lTwo = __vcfsx(__vspltisw(2), 0);
    return Vector3(__vmulfp(lTwo, lRadius));
}

// ===========================================================================
// The four wave-Q5-vtbind bodies. Raw asm: scratchpad/waveQ5/vtbind/asm/0x82BA*.txt
// (from .ida-exports/BURNOUT_X360_ARTIST.XEX -- every per-address JSON existed; no
// headless IDA run was needed for these four).
// ===========================================================================

// ---------------------------------------------------------------------------
// SphereVolume::GetMaximumFeature @ 0x82BA81B0 -- DWARF sphere.h:27. NINE instructions:
//
//   0x82BA81B0  li        r10, 0
//   0x82BA81B4  mr        r11, r3          ; this
//   0x82BA81B8  li        r9,  0x30        ; transform.wAxis (the LOCAL centre row)
//   0x82BA81BC  li        r8,  0x220       ; Feature::pt
//   0x82BA81C0  li        r3,  1           ; return 1
//   0x82BA81C4  stw       r10, 0x230(r6)   ; Feature::numedges = 0   (r6 = &feature)
//   0x82BA81C8  lvx128    v0,  r11, r9
//   0x82BA81CC  stvx128   v0,  r6,  r8     ; Feature::pt = transform.wAxis, ALL 4 LANES
//   0x82BA81D0  blr
//
// A sphere's maximum feature is a POINT (numedges == 0) at its own centre. Neither
// abCcw (r4) nor arDir (r5) is read -- MEASURED: r4 and r5 never appear in the body.
// The centre is the volume's OWN transform row 3, NOT transformed by anything: this
// slot has no transform parameter at all.
//
// ⚠️ The out-parameter arrives in r6 here and in r5 in the byte-identical
// BoxVolume::GetMaximumFeature below. That is the by-reference/by-value `Vector3`
// divergence the DWARF records (sphere.h:27 vs box.h:42) -- see the CollisionVolume.hpp
// VTable banner. On x64 both spellings pass a pointer, so the two host bodies really are
// identical; the console's were not.
// ---------------------------------------------------------------------------
RwBool SphereVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vector3& /*arDir*/,
                                       Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = mTransform.wAxis;
    return 1;
}

// ---------------------------------------------------------------------------
// BoxVolume::GetMaximumFeature @ 0x82BA87F0 -- DWARF box.h:42. Nine instructions,
// instruction for instruction the sphere body above with r5 in place of r6:
//
//   0x82BA87F0  li      r10, 0
//   0x82BA87F4  mr      r11, r3
//   0x82BA87F8  li      r9,  0x30
//   0x82BA87FC  li      r8,  0x220
//   0x82BA8800  li      r3,  1
//   0x82BA8804  stw     r10, 0x230(r5)   ; Feature::numedges = 0   (r5 = &feature)
//   0x82BA8808  lvx128  v0,  r11, r9
//   0x82BA880C  stvx128 v0,  r5,  r8     ; Feature::pt = transform.wAxis
//   0x82BA8810  blr
//
// ⚠️ NOT a "box feature". The box's maximum-feature slot returns the CENTRE POINT, the
// same as the sphere's -- it does not build the three face normals (that is
// GPBox::GetMaximumFeature @0x82BA8918, a different function, reached through
// GPInstance::mMethods, and already bodied in GPBox.cpp). Reconstructing a face feature
// here would be invention.
// ---------------------------------------------------------------------------
RwBool BoxVolume::GetMaximumFeature(RwBool /*abCcw*/, Vector3 /*aDir*/,
                                    Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = mTransform.wAxis;
    return 1;
}

// ---------------------------------------------------------------------------
// SphereVolume::CreateGPInstance @ 0x82BA8100 -- DWARF sphere.h:21.
//
//   lvx128 v0, r3+0x30                      ; the LOCAL centre row
//   cmplwi cr6, r5, 0 ; beq -> skip         ; lpTransform NULL arm
//     vspltw v9/v8/v0 = centre.x/.y/.z
//     v13 = T[0] ; v12 = T[3] ; v11 = T[1] ; v10 = T[2]
//     vmaddfp v13, v9,  v12, v13            ; centre.x*T0 + T3
//     vmaddfp v13, v8,  v13, v11            ; centre.y*T1 + prev
//     vmaddfp v0,  v0,  v13, v10            ; centre.z*T2 + prev   (a POINT transform)
//   lwz  r7, 0x5C(this)                     ; Volume::m_flags
//   stvx128 v0, r0, r4                      ; mPos            (+0x00)
//   stw  r9,  0x84(r4)                      ; mVolumeTag = this
//   stb  0,   0x8C(r4)  / stb 0, 0x8D(r4)   ; mNumFaceNormals = mNumEdgeDirections = 0
//   stw  1,   0x90(r4)                      ; mVolumeType = 1 (GPInstance::SPHERE)
//   stw  0,   0x88(r4)                      ; mUserTag = 0
//   lfs  f0,  0x50(this) ; stfs f0, 0x80(r4); mFatness = radius
//   stw  r7,  0x94(r4)                      ; mFlags
//   4 x lwz/stw from off_82F91900 into r4+0xA4  ; mMethods = the SPHERE VolumeMethods row
//   li r3, 1 ; blr
//
// The sphere writes NO face normals and NO edge directions -- its core primitive is a
// point and the radius rides in mFatness (GPInstance.hpp:198 says the same). The
// mDimensions row is left EXACTLY as the caller's buffer had it; that is not an
// omission, the console never touches +0x70 here.
//
// CHECKLIST A (console literals): `off_82F91900` is `unk_82F918F0 + 1*0x10`, i.e. row 1
// of g_aGPVolumeMethods, and the console copies it as four 4-byte words. The 0x10 row
// stride and the 0xA4 base are CONSOLE-ONLY and stay in this comment: GPInstance::mMethods
// is pointer-widened on x64 (GPInstance.hpp:164), so the host spelling is a whole-struct
// assignment through a typed array index. Same treatment as CapsuleVolume.cpp:161.
// ---------------------------------------------------------------------------
RwBool SphereVolume::CreateGPInstance(GPInstance& arInstance, const Matrix44Affine* lpTransform) const
{
    __vector4 centre = mTransform.wAxis.mV;
    if (lpTransform)
    {
        __vector4 result = __vmaddfp(__vspltw(centre, 0), lpTransform->xAxis.mV, lpTransform->wAxis.mV);
        result = __vmaddfp(__vspltw(centre, 1), lpTransform->yAxis.mV, result);
        centre = __vmaddfp(__vspltw(centre, 2), lpTransform->zAxis.mV, result);
    }
    arInstance.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInstance.mPos.mV = centre;
    arInstance.mNumFaceNormals = 0;
    arInstance.mNumEdgeDirections = 0;
    arInstance.mVolumeType = GPInstance::SPHERE;
    arInstance.mUserTag = 0;
    arInstance.mFatness = mfRadius;
    arInstance.mFlags = muFlags;
    arInstance.mMethods = g_aGPVolumeMethods[GPInstance::SPHERE];
    return 1;
}

// ---------------------------------------------------------------------------
// BoxVolume::CreateGPInstance @ 0x82BA92E8 -- DWARF box.h:36.
//
// Transform arm (0x82BA9308..0x82BA938C): the three basis rows are ROTATED by the
// argument's 3x3 and row 3 is transformed as a POINT, the same fold BoxVolume::GetBBox
// uses -- read off the vmaddfp chain (vmaddfp vD,vA,vB,vC == vA*vC + vB, the operand rule
// this directory states at every site):
//     v0  = row0.z*T2 + (row0.y*T1 + row0.x*T0)          -> rotated row 0
//     v13 = row1.z*T2 + (row1.y*T1 + row1.x*T0)          -> rotated row 1
//     v12 = row2.z*T2 + (row2.y*T1 + row2.x*T0)          -> rotated row 2   (via `vmr v12,v11`)
//     v10 = row3.z*T2 + (row3.y*T1 + (row3.x*T0 + T3))   -> transformed row 3 (a POINT)
// NULL arm (0x82BA9390): the four local rows verbatim, into the SAME four registers.
//
// Then, in store order:
//     stvx128 v10, r0, r4        mPos               (+0x00) = the centre
//     stw     r11, 0x84(r4)      mVolumeTag = this
//     stw     4,   0x90(r4)      mVolumeType = 4  (GPInstance::BOX)
//     stw     0,   0x88(r4)      mUserTag = 0
//     stb     3,   0x8C(r4)      mNumFaceNormals    = 3
//     stb     3,   0x8D(r4)      mNumEdgeDirections = 3
//     stvx128 v0,  r4, 0x10      mFaceNormals[0]    = row0
//     stvx128 v0,  r4, 0x40      mEdgeDirections[0] = row0      <-- the SAME row twice
//     stvx128 v13, r4, 0x20      mFaceNormals[1]    = row1
//     stvx128 v13, r4, 0x50      mEdgeDirections[1] = row1
//     stvx128 v12, r4, 0x30      mFaceNormals[2]    = row2
//     stvx128 v12, r4, 0x60      mEdgeDirections[2] = row2
//     stvx128 v11, r4, 0x70      mDimensions        = (hx, hy, hz, hx)
//     lfs f0, 0x50(r11) ; stfs f0, 0x80(r4)   mFatness = radius
//     stw r5, 0x94(r4)                        mFlags   = m_flags (+0x5C, loaded early)
//     4 x lwz/stw from off_82F91930 into r4+0xA4   mMethods = the BOX VolumeMethods row
//     li r3, 1
//
// mDimensions' W LANE IS hx, MEASURED, not padding: v9/v11/v8 are lane-0 broadcasts of
// +0x44/+0x48/+0x4C and the gather is `vperm v11, v9, v11, unk_82CDA350` followed by
// `vrlimi128 v11, v8, 2, 0`. unk_82CDA350 is DUMPED (TriangleVolume_wN_01.cpp:176-182):
// 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03, i.e. vperm(vA,vB,ctl) =
// (vA.x, vB.y, vA.x, vA.x); the vrlimi then overwrites lane z. With broadcast sources
// that yields (hx, hy, hz, hx) -- the same control and the same conclusion the triangle's
// CreateGPInstance already records for its edge lengths.
//
// CHECKLIST A: `off_82F91930` is `unk_82F918F0 + 4*0x10`, row 4 (BOX) of
// g_aGPVolumeMethods; the 0x10 stride and the 0xA4 base are console-only (see the sphere
// body above).
// ---------------------------------------------------------------------------
RwBool BoxVolume::CreateGPInstance(GPInstance& arInstance, const Matrix44Affine* lpTransform) const
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
    const __vector4 hx = __vspltw(__lvlx(&mBoxData.mfHx, 0), 0);
    const __vector4 hy = __vspltw(__lvlx(&mBoxData.mfHy, 0), 0);
    const __vector4 hz = __vspltw(__lvlx(&mBoxData.mfHz, 0), 0);
    __vector4 dims = __vrlimi(hx, hy, 4, 0);
    dims = __vrlimi(dims, hz, 2, 0);
    arInstance.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInstance.mPos.mV = lw;
    arInstance.mVolumeType = GPInstance::BOX;
    arInstance.mUserTag = 0;
    arInstance.mNumFaceNormals = 3;
    arInstance.mNumEdgeDirections = 3;
    arInstance.mFaceNormals[0].mV = lx;
    arInstance.mEdgeDirections[0].mV = lx;
    arInstance.mFaceNormals[1].mV = ly;
    arInstance.mEdgeDirections[1].mV = ly;
    arInstance.mFaceNormals[2].mV = lz;
    arInstance.mEdgeDirections[2].mV = lz;
    arInstance.mDimensions.mV = dims;
    arInstance.mFatness = mfRadius;
    arInstance.mFlags = muFlags;
    arInstance.mMethods = g_aGPVolumeMethods[GPInstance::BOX];
    return 1;
}

} // namespace collision
} // namespace rw

// ARTIST folds these genuine per-type methods to 82BAC980, 82B0F1B8 or 82AD5078.
namespace rw { namespace collision {
RwBool BoxVolume::GetInterval(Vector3, Interval&) const { return 1; }
void BoxVolume::Release() {}
RwBool SphereVolume::GetInterval(const Vector3&, Interval&) const { return 1; }
void SphereVolume::Release() {}
} }
