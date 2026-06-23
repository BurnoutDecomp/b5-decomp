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
// vsubfp / vmaxfp / vrefp + Newton-Raphson refine / fsel / vandc sign-masking),
// so -- following the project's FeatureEdge / Triangle4 precedent -- they are
// lowered to a SEMANTIC, portable, named-member float reconstruction rather than
// an inline-asm transcription. The store ORDER and offsets are preserved.
//
// The asm stores four 16-byte rows into the object:
//     +0x00  mClipMin   : the per-axis lower clip coordinate (start - padded)
//     +0x10  mClipMax   : the per-axis upper clip coordinate (end + padded)
//     +0x20  mRecipSpan  : 1 / (mClipMax - mClipMin), reciprocal refined with a
//                          vrefp + two Newton-Raphson steps
//     +0x30  mPadExtent  : the segment direction (delta) plus the robustness pad
//
// Several .rdata constants are reached only by reference in the asm and carry no
// value in the export; they are FLAGGED as inferred (see the .cpp):
//     flt_820F2708, flt_820AD47C  -- robustness epsilons
//     flt_82001C98, flt_820037C8  -- the fsel sign-select pair
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

    // @ 0x828AED60 -- fill the per-axis clip parameters. Returns this. The asm
    // takes r3=this, r4=lpBox and the segment in the VMX argument registers
    // (rStart=v1, rEnd=v2, rDir=v3).
    AALineClipper* Init(const Vec4& rStart, const Vec4& rEnd, const Vec4& rDir,
                        const Aabb* lpBox);

    // .rdata constants reached only by reference (no value in the export).
    // FLAGGED as inferred.
    static const f32 KF_PAD_EPSILON;     // flt_820F2708 (Init robustness pad)
    static const f32 KF_ABS_EPSILON;     // flt_820AD47C (abs-extent epsilon)
    static const f32 KF_FSEL_POS;        // flt_82001C98 (fsel value for sign>=0)
    static const f32 KF_FSEL_NEG;        // flt_820037C8 (fsel value for sign<0)

    Vec4 mClipMin;     // +0x00
    Vec4 mClipMax;     // +0x10
    Vec4 mRecipSpan;   // +0x20
    Vec4 mPadExtent;   // +0x30
};

} // namespace collision
} // namespace rw
