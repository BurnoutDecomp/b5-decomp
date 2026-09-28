#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::AALineClipper -- precomputes the per-axis parameters used to
// clip a line segment against axis-aligned boxes during a RenderWare Kd-tree
// line query (KdTreeLineQuery / CylinderVolume::FatLineSegIntersect build one).
//
// OWNING HOME for the two functions the X360 binary defines:
//     rw::collision::AALineClipper::AALineClipper  @ 0x82BAE3C8  (ctor)
//     rw::collision::AALineClipper::Init           @ 0x828AED60
//
// No DWARF hints exist for this TU, so the LAYOUT below is
// reconstructed from the X360 VMX asm. The bodies are hand-vectorised (lvx128 /
// vsubfp / vmaxfp / vrefp + Newton-Raphson refine / fsel / vandc sign-masking);
// since 2026-09-28 (lane L2, stage (c)) they are computed lane by lane as the
// console computes them, every .rdata word read from the image (see the .cpp;
// tests/run_l2_aalineclipper.py replays the ARTIST words run on emu64).
//
// The asm stores four 16-byte rows into the object:
//     +0x00  mClipMin   : the per-axis lower clip coordinate (start - signed pad)
//     +0x10  mSpan      : (end + signed pad) - mClipMin
//     +0x20  mRecipSpan : 1 / mSpan, vrefp refined by two Newton-Raphson steps
//     +0x30  mPadExtent : the seed (v3) plus the pad's growth
//
// .rdata words (tools/re/x360rd.py):
//     flt_820F2708 == 0.5   -- the scale of the segment's delta
//     flt_820AD47C == 1e-6  -- the pad's floor, relative to the largest |coordinate|
//     flt_82001C98 == 1.0, flt_820037C8 == -1.0  -- the fsel sign-select pair
// ===========================================================================

namespace rw
{
namespace collision
{

class AALineClipper
{
public:
    // A 16-byte (xyzw) vector row matching a VMX register lane set.
    struct Vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // The axis-aligned box the segment is clipped against (min/max corners).
    struct Aabb
    {
        Vec4 mMin;   // +0x00
        Vec4 mMax;   // +0x10
    };

    // @ 0x82BAE3C8 -- construct from a line segment and a box, delegating to
    // Init. X360 __fastcall: r3=this, r4=lpBox; the segment endpoints arrive in
    // the VMX argument registers (rStart=v1, rEnd=v2). The ctor SYNTHESISES Init's
    // dir-seed (v3) itself -- it is NOT a parameter: per lane it computes
    // max(|start|,|end|) * KF_ABS_EPSILON (flt_820AD47C). See the .cpp.
    AALineClipper(const Vec4& rStart, const Vec4& rEnd, const Aabb* lpBox);

    // ADDITIVE GROW (rw-physics-collision group): a trivial default ctor so the
    // clipper can be embedded by value in query objects (KdTreeLineQuery) that seed
    // it post-construction via Init. The X360 leaves the rows uninitialised before
    // Init runs; this matches that (no zeroing). Purely additive -- no member or
    // existing ctor changes.
    AALineClipper() = default;

    // @ 0x828AED60 -- fill the per-axis clip parameters. Returns this. The asm
    // takes r3=this, r4=lpBox and the segment in the VMX argument registers
    // (rStart=v1, rEnd=v2, rDir=v3).
    AALineClipper* Init(const Vec4& rStart, const Vec4& rEnd, const Vec4& rDir,
                        const Aabb* lpBox);

    // .rdata words (tools/re/x360rd.py).
    static const f32 KF_HALF_DELTA;      // flt_820F2708 == 0x3F000000 (0.5: the delta's scale)
    static const f32 KF_ABS_EPSILON;     // flt_820AD47C == 0x358637BD (1e-6: the pad floor's scale)
    static const f32 KF_FSEL_POS;        // flt_82001C98 == 0x3F800000 (fsel value for sign >= 0)
    static const f32 KF_FSEL_NEG;        // flt_820037C8 == 0xBF800000 (fsel value for sign < 0)

    Vec4 mClipMin;     // +0x00
    Vec4 mSpan;        // +0x10
    Vec4 mRecipSpan;   // +0x20
    Vec4 mPadExtent;   // +0x30
};

} // namespace collision
} // namespace rw
