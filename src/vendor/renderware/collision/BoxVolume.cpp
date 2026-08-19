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
// NOT LANDED (separate ledger rows, no body anywhere in the tree, listed so the next
// owner does not re-derive that they are missing):
//   rw::collision::BoxVolume::LineSegIntersect     @ 0x82BA9478
//   rw::collision::SphereVolume::LineSegIntersect  @ 0x82BA82C8
// They are the remaining descriptor slots for types 1 and 4; the four GetBBox/
// GetBBoxDiag slots that scratchpad/waveQ2/rwvol.owner.md section 7 item (c) lists as
// un-homed are all closed by this file.
//
// ---------------------------------------------------------------------------------------
// GROWN 2026-08-19 (wave Q5 vtbind) -- four more bodies, so that the SPHERE and BOX
// descriptor records in VolumeVTables.cpp can bind every slot the shipped image binds
// except lineSegIntersect:
//
//   rw::collision::SphereVolume::GetMaximumFeature @ 0x82BA81B0   (9 insns)
//   rw::collision::BoxVolume::GetMaximumFeature    @ 0x82BA87F0   (9 insns)
//   rw::collision::SphereVolume::CreateGPInstance  @ 0x82BA8100   (~40 insns)
//   rw::collision::BoxVolume::CreateGPInstance     @ 0x82BA92E8   (~85 insns)
//
// STILL NOT LANDED, and WHY (parked with a reason, per AGENTS gotcha 8 -- raw asm dumped
// to scratchpad/waveQ5/vtbind/asm/ so the next owner does not re-export it):
//   rw::collision::SphereVolume::LineSegIntersect  @ 0x82BA82C8  136 insns. A full
//       ray/sphere kernel around rwcSphereLineSegIntersect @0x82BA81D8 (which IS bodied,
//       LineSegIntersect.cpp:700) plus TWO hand-written Newton-Raphson refinement chains
//       (vrefp then two vnmsubfp/vmaddfp rounds for the 1/t reciprocal, and vrsqrtefp +
//       two rounds for the normal's normalise) and a vcmpgtfp./mfocrf CR-bit predicate.
//       Landing it faithfully is a body-sized job of its own, and NOTHING on the wave-Q5
//       smash-gate path calls it: the volume line query is a different consumer
//       (rw::collision::VolumeLineQuery, still a link stub in AptRenderLinkStubs.cpp).
//   rw::collision::BoxVolume::LineSegIntersect     @ 0x82BA9478  723 insns -- the largest
//       body in this directory after the cylinder SAT helper. Six slab/cap arms calling
//       rwcSphereLineSegIntersect, rwcPlaneLineSegIntersect @0x82BA8818 (NO body in the
//       tree -- a second, larger hole behind it) and rwcCylinderLineSegIntersect, with
//       __savevmx_122 hand register allocation. Same "no caller on this path" argument.
// Both descriptor slots are therefore left NULL in VolumeVTables.cpp with their X360
// addresses named, which is an honest hole -- not a stub that returns a wrong answer.
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
    // CONSEQUENCE (rewritten 2026-08-19 -- the previous text said this left consumers
    // broken; it does not): the enum-in-+0x40 representation is the DESIGNED HOST FORM,
    // not a shortfall. Every consumer that dispatches recovers the descriptor as
    // gVolumeVTable[enum] through the shared helper GetVolumeDescriptor
    // (CollisionVolume.hpp:450-453) -- VolumeBBoxQuery.cpp:83, VolumeQuery.cpp:178 and
    // PrimitiveIntersect.cpp:1259 each go through it. rw::collision::Volume::
    // InitializeVTable is static, real and LIVE (SDKs/EATech/rwcollision/volume.cpp; its
    // WorldLinkStubs `return 0` gate was retired 2026-08-18), so gVolumeVTable is filled,
    // not all-zero: it holds the six descriptor records defined in
    // vendor/renderware/collision/VolumeVTables.cpp, whose method slots are bound (34 of
    // the 40 the image binds; 2 genuine image zeros, 6 parked with per-slot reasons).
    // No host-width promotion is required -- FixableVolume::FixUp/FixDown are the
    // console's validation half only, and the enum<->pointer swap is the identity here.
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
    // (SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h:39 vs :232). The
    // original reason was an LNK2005: the ctor spelling emits a COMDAT
    // `rw::math::vpu::Vector3::Vector3(float,float,float)` that collided with the strong
    // out-of-line copy in SDKs/EATech/rw/math/vpu/vector3.cpp.
    // ⚠️ THAT REASON IS SPENT (corrected 2026-08-19, wave Q5 vtbind -- a wrong comment is a
    // real defect): the wave-Q5 integration DELETED that bat line, and build_game_exe.bat
    // now says so at :2610. The spelling is kept anyway because `Set` is the direction
    // scratchpad/waveQ2/rwvol.owner.md section 4.5 item 3 wants these sites moved in, and
    // because switching them would change nothing observable --
    //
    // ⚠️ NEITHER SPELLING WRITES LANE 3. Both `Set` and the 3-float ctor leave the W lane of
    // mMin/mMax EXACTLY as the caller's buffer had it, while the console's `stvx128` stores
    // all sixteen bytes (its W lane is row3.w -/+ extent.w). MEASURED at run time: the
    // wave-Q5 descriptor probe compared a zeroed AABBox against a stack-garbage one and the
    // ONLY difference was those two words. Nothing geometric reads them today, but any
    // future memcmp/hash of an AABBox (a cache key, a dedup, a replay checksum) would be
    // non-deterministic. The fix is a tree-wide one in the EATech Vector3 vocabulary, not
    // here -- reported in scratchpad/waveQ5/vtbind.owner.md.
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

    // `Set` for the same reason as BoxVolume::GetBBox above -- including the W-lane
    // caveat recorded there.
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
RwBool SphereVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vec4& /*arDir*/,
                                       Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = maTransform[3];
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
RwBool BoxVolume::GetMaximumFeature(RwBool /*abCcw*/, Vec4 /*aDir*/,
                                    Feature& arFeature) const
{
    arFeature.numedges = 0;
    arFeature.pt       = maTransform[3];
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
RwBool SphereVolume::CreateGPInstance(GPInstance& arInstance, const Vec4* lpTransform) const
{
    Vec4 lvCentre;   // v0 -- the (optionally transformed) centre

    if (lpTransform != 0)
    {
        lvCentre = TransformPoint4(maTransform[3], lpTransform);
    }
    else
    {
        lvCentre = maTransform[3];
    }

    // FLAG (host width), identical to CapsuleVolume.cpp:180: the console stores its
    // 4-byte `this` straight into the u32 mVolumeTag word and the batch kernels copy that
    // word verbatim into PrimitivePairIntersectResult::v1/v2, so this truncates on x64.
    // The fix is to promote GPInstance::mVolumeTag (and PPIR::v1/v2) to pointer width,
    // which belongs to the GPInstance TU, not here.
    arInstance.mVolumeTag         = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInstance.mPos               = lvCentre;
    arInstance.mNumFaceNormals    = 0;
    arInstance.mNumEdgeDirections = 0;
    arInstance.mVolumeType        = GPInstance::SPHERE;   // stw 1, 0x90
    arInstance.mUserTag           = 0;
    arInstance.mFatness           = mfRadius;
    arInstance.mFlags             = muFlags;
    arInstance.mMethods           = g_aGPVolumeMethods[GPInstance::SPHERE];

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
RwBool BoxVolume::CreateGPInstance(GPInstance& arInstance, const Vec4* lpTransform) const
{
    Vec4 lvRow0;   // v0  -- rotated  transform.xAxis
    Vec4 lvRow1;   // v13 -- rotated  transform.yAxis
    Vec4 lvRow2;   // v12 -- rotated  transform.zAxis
    Vec4 lvRow3;   // v10 -- transformed transform.wAxis (the centre)

    if (lpTransform != 0)
    {
        lvRow0 = RotateRow4(maTransform[0], lpTransform);
        lvRow1 = RotateRow4(maTransform[1], lpTransform);
        lvRow2 = RotateRow4(maTransform[2], lpTransform);
        lvRow3 = TransformPoint4(maTransform[3], lpTransform);
    }
    else
    {
        lvRow0 = maTransform[0];
        lvRow1 = maTransform[1];
        lvRow2 = maTransform[2];
        lvRow3 = maTransform[3];
    }

    // FLAG (host width) -- see SphereVolume::CreateGPInstance above.
    arInstance.mVolumeTag         = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInstance.mPos               = lvRow3;
    arInstance.mVolumeType        = GPInstance::BOX;      // stw 4, 0x90
    arInstance.mUserTag           = 0;
    arInstance.mNumFaceNormals    = 3;
    arInstance.mNumEdgeDirections = 3;

    arInstance.mFaceNormals[0]    = lvRow0;
    arInstance.mEdgeDirections[0] = lvRow0;
    arInstance.mFaceNormals[1]    = lvRow1;
    arInstance.mEdgeDirections[1] = lvRow1;
    arInstance.mFaceNormals[2]    = lvRow2;
    arInstance.mEdgeDirections[2] = lvRow2;

    arInstance.mDimensions.x = mBoxData.mfHx;
    arInstance.mDimensions.y = mBoxData.mfHy;
    arInstance.mDimensions.z = mBoxData.mfHz;
    arInstance.mDimensions.w = mBoxData.mfHx;   // the dumped vperm control, see above

    arInstance.mFatness = mfRadius;
    arInstance.mFlags   = muFlags;
    arInstance.mMethods = g_aGPVolumeMethods[GPInstance::BOX];

    return 1;
}

} // namespace collision
} // namespace rw
