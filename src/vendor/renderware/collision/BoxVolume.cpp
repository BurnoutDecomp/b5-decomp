#include "vendor/renderware/collision/CollisionVolume.hpp"

#include "vendor/renderware/collision/AABBox.hpp"   // AABBox (the GetBBox out-parameter)
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
// The console does the arithmetic on all FOUR lanes and stores 16-byte rows; the W lane
// is inert here (the box half-extents and the fatness are scalars broadcast with
// `lvlx`+`vspltw`, and every consumer reads x/y/z), so the reconstruction carries the
// three geometric lanes exactly as its four siblings in this directory do.
//
// NOT LANDED (separate ledger rows, no body anywhere in the tree, listed so the next
// owner does not re-derive that they are missing):
//   rw::collision::BoxVolume::GetMaximumFeature    @ 0x82BA87F0
//   rw::collision::BoxVolume::CreateGPInstance     @ 0x82BA92E8
//   rw::collision::BoxVolume::LineSegIntersect     @ 0x82BA9478
//   rw::collision::SphereVolume::CreateGPInstance  @ 0x82BA8100
//   rw::collision::SphereVolume::GetMaximumFeature @ 0x82BA81B0
//   rw::collision::SphereVolume::LineSegIntersect  @ 0x82BA82C8
// They are the remaining descriptor slots for types 1 and 4; the four GetBBox/
// GetBBoxDiag slots that scratchpad/waveQ2/rwvol.owner.md section 7 item (c) lists as
// un-homed are all closed by this file.
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
    // ⚠️ [FLAG host-width delta -- the ONE store that is not console-exact.]
    // The console stamp is `lwz r11, gVolumeVTable[type] ; stw r11, 0x40(vol)`: the
    // RUNTIME per-type descriptor POINTER (BoxVolume reads dword_8327EEF0 == slot 4,
    // SphereVolume reads dword_8327EEE4 == slot 1, where dword_8327EEE0 is
    // rw::collision::gVolumeVTable, SDKs/EATech/rwcollision/volume.cpp:100). That
    // pointer is 4 bytes on the console and 8 on the host, and the slot CANNOT widen --
    // Volume::mfRadius/muGroupID/muSurfaceID/muFlags and the +0x44 half-extents are
    // offset-pinned by the static_asserts in CollisionVolume.hpp and by the 96-byte
    // serialised stride. Storing a truncated host pointer there is the exact
    // console-value corruption class this project has shipped repeatedly, so it is not
    // done. The EVolumeType id is stamped instead: that is the value this same slot
    // legitimately carries in this tree's own *unfixed* state, defined by
    // BrnPhysics::Props::FixableVolume::FixUp @0x828A87A0 (reads the enum here, writes
    // gVolumeVTable[enum] back) and FixDown @0x828A8830 (reverses it), so one FixUp()
    // call converts the record the day the slot can hold a host pointer.
    // CONSEQUENCE, stated rather than hidden: a consumer that DISPATCHES through +0x40
    // (VolumeBBoxQuery.cpp:78, VolumeQuery.cpp:173, PrimitiveIntersect.cpp:825) sees the
    // id, not a descriptor. That is not a regression on this build -- gVolumeVTable is
    // ALL ZERO at run time today because Volume::InitializeVTable's real body is dead
    // code (the static/non-static mangling split documented in
    // volume_debug_access.h:280-304), so the console-exact value would be NULL here.
    // The fix is the Volume/InitializeVTable host-width promotion, reported to the
    // conductor; it is not this file's to make.
    void ConstructVolume(Volume& arVolume, EVolumeType aeType)
    {
        arVolume.mfRadius    = KF_ZERO_FATNESS;
        arVolume.muGroupID   = 0;
        arVolume.muSurfaceID = 0;
        arVolume.muFlags     = KU_VOLUMEFLAG_ISENABLED;

        arVolume.muVTableSlot = static_cast<u32>(aeType);   // [FLAG host-width delta]

        arVolume.maTransform[0] = KV_BASIS_X;
        arVolume.maTransform[3] = KV_ZERO;
        arVolume.maTransform[1] = KV_BASIS_Y;
        arVolume.maTransform[2] = KV_BASIS_Z;
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
BoxVolume::BoxVolume(const Vec4& arHalfDimensions)
{
    ConstructVolume(*this, E_VOLUMETYPE_BBOX);

    mBoxData.mfHx = arHalfDimensions.x;
    mBoxData.mfHy = arHalfDimensions.y;
    mBoxData.mfHz = arHalfDimensions.z;
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
                                 const Vec4&           arHalfDimensions)
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
RwBool BoxVolume::GetBBox(const Vec4* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    f32 lfRow0X, lfRow0Y, lfRow0Z;
    f32 lfRow1X, lfRow1Y, lfRow1Z;
    f32 lfRow2X, lfRow2Y, lfRow2Z;
    f32 lfRow3X, lfRow3Y, lfRow3Z;

    if (lpTransform != 0)
    {
        RotateRow(maTransform[0], lpTransform, lfRow0X, lfRow0Y, lfRow0Z);
        RotateRow(maTransform[1], lpTransform, lfRow1X, lfRow1Y, lfRow1Z);
        RotateRow(maTransform[2], lpTransform, lfRow2X, lfRow2Y, lfRow2Z);
        TransformPoint(maTransform[3], lpTransform, lfRow3X, lfRow3Y, lfRow3Z);
    }
    else
    {
        lfRow0X = maTransform[0].x;  lfRow0Y = maTransform[0].y;  lfRow0Z = maTransform[0].z;
        lfRow1X = maTransform[1].x;  lfRow1Y = maTransform[1].y;  lfRow1Z = maTransform[1].z;
        lfRow2X = maTransform[2].x;  lfRow2Y = maTransform[2].y;  lfRow2Z = maTransform[2].z;
        lfRow3X = maTransform[3].x;  lfRow3Y = maTransform[3].y;  lfRow3Z = maTransform[3].z;
    }

    // Half extent per axis: |row0|*hx + |row1|*hy + |row2|*hz + the isotropic fatness.
    const f32 lfExtentX = std::fabs(lfRow0X) * mBoxData.mfHx
                        + std::fabs(lfRow1X) * mBoxData.mfHy
                        + std::fabs(lfRow2X) * mBoxData.mfHz + mfRadius;
    const f32 lfExtentY = std::fabs(lfRow0Y) * mBoxData.mfHx
                        + std::fabs(lfRow1Y) * mBoxData.mfHy
                        + std::fabs(lfRow2Y) * mBoxData.mfHz + mfRadius;
    const f32 lfExtentZ = std::fabs(lfRow0Z) * mBoxData.mfHx
                        + std::fabs(lfRow1Z) * mBoxData.mfHy
                        + std::fabs(lfRow2Z) * mBoxData.mfHz + mfRadius;

    // `Set` rather than `mMin = Vector3(x,y,z)` -- SAME three lane stores
    // (SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h:39 vs :232), but the
    // ctor spelling would emit a COMDAT `rw::math::vpu::Vector3::Vector3(float,float,float)`
    // that LNK2005s against the strong out-of-line copy in
    // SDKs/EATech/rw/math/vpu/vector3.cpp (mounted at build_game_exe.bat:2562). That is the
    // collision scratchpad/waveQ5/rwcoll.owner.md section 3.1 measured for
    // CapsuleVolume/CylinderVolume/TriangleVolume; avoiding it here keeps THIS TU's mount
    // from depending on that bat-line deletion. (The Vector3 ctor fork is still a real
    // defect -- reported, not this cluster's to fix.)
    arResult.mMin.Set(lfRow3X - lfExtentX, lfRow3Y - lfExtentY, lfRow3Z - lfExtentZ);
    arResult.mMax.Set(lfRow3X + lfExtentX, lfRow3Y + lfExtentY, lfRow3Z + lfExtentZ);
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
Vec4 BoxVolume::GetBBoxDiag() const
{
    const Vec4& lvRow0 = maTransform[0];
    const Vec4& lvRow1 = maTransform[1];
    const Vec4& lvRow2 = maTransform[2];

    const f32 lfDiagX = 2.0f * (std::fabs(lvRow0.x) * mBoxData.mfHx
                              + std::fabs(lvRow1.x) * mBoxData.mfHy
                              + std::fabs(lvRow2.x) * mBoxData.mfHz + mfRadius);
    const f32 lfDiagY = 2.0f * (std::fabs(lvRow0.y) * mBoxData.mfHx
                              + std::fabs(lvRow1.y) * mBoxData.mfHy
                              + std::fabs(lvRow2.y) * mBoxData.mfHz + mfRadius);
    const f32 lfDiagZ = 2.0f * (std::fabs(lvRow0.z) * mBoxData.mfHx
                              + std::fabs(lvRow1.z) * mBoxData.mfHy
                              + std::fabs(lvRow2.z) * mBoxData.mfHz + mfRadius);

    const Vec4 lvDiag = { lfDiagX, lfDiagY, lfDiagZ, 0.0f };
    return lvDiag;
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

    ConstructVolume(*lpVolume, E_VOLUMETYPE_SPHERE);
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
RwBool SphereVolume::GetBBox(const Vec4* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    f32 lfCentreX;
    f32 lfCentreY;
    f32 lfCentreZ;

    if (lpTransform != 0)
    {
        TransformPoint(maTransform[3], lpTransform, lfCentreX, lfCentreY, lfCentreZ);
    }
    else
    {
        lfCentreX = maTransform[3].x;
        lfCentreY = maTransform[3].y;
        lfCentreZ = maTransform[3].z;
    }

    // `Set` for the same LNK2005 reason as BoxVolume::GetBBox above.
    arResult.mMin.Set(lfCentreX - mfRadius, lfCentreY - mfRadius, lfCentreZ - mfRadius);
    arResult.mMax.Set(lfCentreX + mfRadius, lfCentreY + mfRadius, lfCentreZ + mfRadius);
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
Vec4 SphereVolume::GetBBoxDiag() const
{
    const f32 lfDiameter = 2.0f * mfRadius;

    const Vec4 lvDiag = { lfDiameter, lfDiameter, lfDiameter, 0.0f };
    return lvDiag;
}

} // namespace collision
} // namespace rw
