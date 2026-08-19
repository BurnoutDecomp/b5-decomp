#include "vendor/renderware/collision/TriangleVolume.hpp"

#include "vendor/renderware/collision/GPInstance.hpp"   // GPInstance (CreateGPInstance)

#include <cmath>     // sqrt (the vrsqrtefp/vrefp + Newton-Raphson de-optimisation)
#include <cstdint>   // uintptr_t (the console 32-bit mVolumeTag pointer image)

// ===========================================================================
// PARTFILE -- rw::collision::TriangleVolume::CreateGPInstance @ 0x82BBAA00.
//
// MERGE NOTE for the conductor: this body belongs in the committed TU home
// b5-decomp/src/vendor/renderware/collision/TriangleVolume.cpp. On merge:
//   * delete the anonymous-namespace helpers below -- Dot3 / TransformPoint /
//     TransformDirection / SubXYZW are BYTE-IDENTICAL copies of the ones
//     already in TriangleVolume.cpp:56-108 (duplicated here only because they
//     have internal linkage and this is a separate TU);
//   * add `#include "vendor/renderware/collision/GPInstance.hpp"` and
//     `#include <cstdint>` to TriangleVolume.cpp's include set (<cmath> and
//     <cstring> are already there);
//   * move the `extern const GPInstance::VolumeMethods g_aGPVolumeMethods[...]`
//     declaration in (copied verbatim from CapsuleVolume.cpp:65-66 -- it has
//     NOT been hoisted into GPInstance.hpp yet, verified by grep);
//   * rewrite TriangleVolume.cpp's top-of-file note (lines 17-24, "Implementation
//     pending") into the normal homed-function wording -- CreateGPInstance is
//     no longer pending. The KF_DEGENERATE_EPSILON / KU_TRIANGLE_VOLUME_TYPE
//     definitions stay where they are; nothing here duplicates them.
//
// Sources: the RAW asm of 0x82BBAA00 (.ida-exports) plus the recovered tables
// in scratchpad/waveN/TriangleVolume.spec.md. Same VMX-to-scalar treatment as
// the rest of this directory (FeatureEdge.cpp / GPSphere.cpp precedent).
// ===========================================================================

namespace rw
{
namespace collision
{

// ---------------------------------------------------------------------------
// X360 unk_82F918F0 -- the per-VolumeType GP callback table (the canonical
// static GPInstance::sVolumeMethods[6], DWARF volume.h:484). Declaration copied
// verbatim from CapsuleVolume.cpp:65-66; the contents are recovered and are
// documented there (and independently re-dumped in the spec, byte-identical).
// Row [3] TRIANGLE = { GPTriangle::GetMaximumFeature @0x82BBA158,
//                      GPTriangle::GetInterval       @0x82BBA6A0,
//                      GPTriangle::GetIntervals      @0x82BBA6E0,
//                      STUB (lone blr)               @0x82AD5078 }.
// The DEFINITION belongs to the un-recovered GP registration TU.
// FLAGGED: definition pending -> link blocker until that TU lands.
extern const GPInstance::VolumeMethods
    g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];   // X360 unk_82F918F0

namespace
{
    // --- MERGE: identical copies of TriangleVolume.cpp's helpers ------------

    // dot3 of the xyz lanes (the asm's vmsum3fp128 / lane-broadcast fold).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // Affine transform of a point by four Vec4 rows (rows 0..2 the basis, row 3
    // the translation): the vmaddfp chain
    //   out = row3 + row0*p.x + row1*p.y + row2*p.z.
    inline Vec4 TransformPoint(const Vec4* lapRows, const Vec4& arP)
    {
        Vec4 r;
        r.x = lapRows[3].x + lapRows[0].x * arP.x + lapRows[1].x * arP.y + lapRows[2].x * arP.z;
        r.y = lapRows[3].y + lapRows[0].y * arP.x + lapRows[1].y * arP.y + lapRows[2].y * arP.z;
        r.z = lapRows[3].z + lapRows[0].z * arP.x + lapRows[1].z * arP.y + lapRows[2].z * arP.z;
        r.w = lapRows[3].w + lapRows[0].w * arP.x + lapRows[1].w * arP.y + lapRows[2].w * arP.z;
        return r;
    }

    // Rotate a direction by the 3x3 (no translation): the GetNormal transform
    // chain out = row0*n.x + row1*n.y + row2*n.z.
    inline Vec4 TransformDirection(const Vec4* lapRows, const Vec4& arN)
    {
        Vec4 r;
        r.x = lapRows[0].x * arN.x + lapRows[1].x * arN.y + lapRows[2].x * arN.z;
        r.y = lapRows[0].y * arN.x + lapRows[1].y * arN.y + lapRows[2].y * arN.z;
        r.z = lapRows[0].z * arN.x + lapRows[1].z * arN.y + lapRows[2].z * arN.z;
        r.w = lapRows[0].w * arN.x + lapRows[1].w * arN.y + lapRows[2].w * arN.z;
        return r;
    }

    inline Vec4 SubXYZW(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x - b.x;
        r.y = a.y - b.y;
        r.z = a.z - b.z;
        r.w = a.w - b.w;
        return r;
    }
}

// ---------------------------------------------------------------------------
// TriangleVolume::CreateGPInstance @ 0x82BBAA00
//
// r3/r30 = this, r4/r31 = &arInst, r5/r29 = lpTransform (may be NULL). No NULL
// guard on r4 -- the console dereferences it unconditionally. Returns 1
// (li r3, 1 @ 0x82BBABB8).
//
// PROLOGUE, dead code (measured, NOT reproduced): the entry block loads the
// four .rdata identity rows (w::math::vpu::detail::gIVector @0x82181500 =
// (1,0,0,0), unk_82181510 = (0,1,0,0), unk_82181520 = (0,0,1,0),
// unk_82181530 = (0,0,0,1)) into v124/v123/v122/v121. Both paths overwrite or
// ignore them -- an if-conversion artifact of an inlined default-frame ctor --
// so the reconstruction drops them. (Contents dumped 2026-08-04; see the spec.)
//
//   0x82BBAA60  beq cr6, loc_82BBAA90        ; lpTransform == NULL -> skip
//   [transform path] lvx128 v124/v123/v122/v121 <- lpTransform[0..3], then
//     vrlimi128 v124,v0(0.0),1,0             ; row0.w = 0     (mask 1 = w lane)
//     vrlimi128 v123,v0(0.0),1,0             ; row1.w = 0
//     vrlimi128 v122,v0(0.0),1,0             ; row2.w = 0
//     vrlimi128 v121,v13(1.0),1,0            ; row3.w = 1     (vcfsx v13,1,0)
//   The w masking is REAL and is reproduced below: it forces P'.w = 1 and
//   n'.w = 0 no matter what the caller's rows carry in w.
//
//   0x82BBAA94  v127 = maVerts[0]  (+0x00)
//   0x82BBAA9C  v126 = maVerts[1]  (+0x10)
//   0x82BBAAA4  v125 = maVerts[2]  (+0x20)
//   0x82BBAAA8  bl GetNormal(this, &var_C0, r5 = 0)   ; UNTRANSFORMED -- this
//               function applies the transform itself. v0 = the local normal.
//               (The call also clears this->mFlags bit 0x2, which is why the
//               mFlags read at 0x82BBAB4C must happen AFTER it.)
//
//   [transform path, 0x82BBAABC-0x82BBAB30] accumulator starts at row3 for the
//   three points (vmr128 v9/v13 <- v121 taken BEFORE v121 is reused as the P2
//   accumulator), and at row0*n.x (vmulfp128, no row3) for the normal:
//     P0' = row3 + row0*P0.x + row1*P0.y + row2*P0.z   (v9  -> v127)
//     P1' = row3 + row0*P1.x + row1*P1.y + row2*P1.z   (v13 -> v126)
//     P2' = row3 + row0*P2.x + row1*P2.y + row2*P2.z   (v121-> v125)
//     n'  =        row0*n.x  + row1*n.y  + row2*n.z    (v12 -> v0)
//   vmaddfp vD,vA,vB,vC == vA*vC + vB in this family's asm (attested again here
//   by the Newton-Raphson idioms below, which only close under that reading).
//
//   0x82BBAB34  vsubfp128 v9,  v125, v127   ; e0 = P2' - P0'
//   0x82BBAB3C  vsubfp128 v8,  v126, v125   ; e1 = P1' - P2'
//   0x82BBAB44  vsubfp128 v10, v127, v126   ; e2 = P0' - P1'
//   (this order AND these directions are asm-attested, not conventional.)
//
//   store order, verbatim from the asm:
//     stw  r10=3,  0x90(r31)   mVolumeType        = 3 (GPInstance::TRIANGLE)
//     stb  r10=3,  0x8D(r31)   mNumEdgeDirections = 3
//     stvx128 v127, r31+0x00   mPos               = P0'   (all 4 lanes)
//     stb  r9=1,   0x8C(r31)   mNumFaceNormals    = 1
//     stw  r8=0,   0x88(r31)   mUserTag           = 0
//     stw  r30,    0x84(r31)   mVolumeTag         = (console 32-bit) this
//     stvx128 v126, r31+0x20   mFaceNormals[1]    = P1'   } GPTriangle
//     stvx128 v125, r31+0x30   mFaceNormals[2]    = P2'   } vertex aliasing
//     lfs 0x50(r30); stfs 0x80 mFatness           = mfFatness
//     stvx128 v0,   r31+0x10   mFaceNormals[0]    = n'
//     lfs 0x4C(r30); stfs 0xA0 mEdgeData[2]       = mafEdgeCos[2]
//     lfs 0x48(r30); stfs 0x9C mEdgeData[1]       = mafEdgeCos[1]
//     lfs 0x44(r30); stfs 0x98 mEdgeData[0]       = mafEdgeCos[0]
//     lwz 0x5C(r30); stw  0x94 mFlags             = this->mFlags
//     stvx128 v10,  r31+0x70   mDimensions        = (L0, L1, L2, L0)
//     stvx128 v13,  r31+0x40   mEdgeDirections[0] = e0 * (1/L0)
//     stvx128 v12,  r31+0x50   mEdgeDirections[1] = e1 * (1/L1)
//     stvx128 v11,  r31+0x60   mEdgeDirections[2] = e2 * (1/L2)
//
// EDGE LENGTHS (0x82BBAB94-0x82BBAC8C). Per edge i, ai = vmsum3fp128(ei,ei) =
// dot3(ei,ei); the console computes ri = vrsqrtefp(ai) refined by ONE
// Newton-Raphson round (ri' = ri + 0.5*ri*(1 - ai*ri*ri); the 0.5 is
// vcfsx v7,1,1 and the 1.0 is vcfsx v0,1,0), scales each edge by it, then
// recovers Li = vrefp(ri) refined by TWO NR rounds (L' = L + L*(1 - ri*L)).
// Per this family's precedent (FeatureEdge.cpp header comment) those idioms
// de-optimise to the exact real operations: Li = sqrt(dot3(ei,ei)) and
// ei * (1/Li), over ALL FOUR lanes (the w lane rides along).
// There is NO degenerate guard in this function -- unlike GetNormal, which has
// an explicit vcmpgtfp/vsel epsilon test, a zero-length edge here really
// divides by zero on the console too (vrsqrtefp(0) = +inf). None is added.
//
// The three refined values are gathered through unk_82CDA350, DUMPED
// (2026-08-04): bytes 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03,
// i.e. vperm(vA,vB,ctl) = (vA.x, vB.y, vA.x, vA.x); the following
// vrlimi128 ...,2,0 overwrites lane z. Both gathers feed lane-broadcast
// values, so the pair yields (r0, r1, r2, r0) and (L0, L1, L2, L0) --
// mDimensions' w lane is L0 = |P2'-P0'|, MEASURED via that control, and all
// four lanes are written.
//
// CHECKLIST A (console literals): the tail copies four 4-byte words from
// off_82F91920 (= unk_82F918F0 + 3*0x10, the TRIANGLE row) into r31+0xA4, the
// row index having been constant-folded by the compiler. That 0x10 row stride
// and the 0xA4 base are CONSOLE-ONLY and stay in this comment: GPInstance's
// callbacks are pointer-widened on x64 (GPInstance.hpp:164), so this is a
// whole-struct assignment at a typed array index -- never a byte base +
// (type << 4). Same treatment as CapsuleVolume.cpp:154-159.
// ---------------------------------------------------------------------------
RwBool TriangleVolume::CreateGPInstance(GPInstance& arInst, const Vec4* lpTransform) const
{
    // GetNormal is always called with a NULL transform (li r5, 0 @ 0x82BBAA90);
    // this function rotates the result itself with the masked rows below.
    Vec4 lvNormal;
    GetNormal(lvNormal, nullptr);

    Vec4 lvP0;   // v127
    Vec4 lvP1;   // v126
    Vec4 lvP2;   // v125

    if (lpTransform != nullptr)
    {
        // The vrlimi128 w-lane masking: rows 0..2 get w = 0, row 3 gets w = 1.
        const Vec4 laRows[4] = {
            { lpTransform[0].x, lpTransform[0].y, lpTransform[0].z, 0.0f },
            { lpTransform[1].x, lpTransform[1].y, lpTransform[1].z, 0.0f },
            { lpTransform[2].x, lpTransform[2].y, lpTransform[2].z, 0.0f },
            { lpTransform[3].x, lpTransform[3].y, lpTransform[3].z, 1.0f },
        };

        lvP0     = TransformPoint(laRows, maVerts[0]);
        lvP1     = TransformPoint(laRows, maVerts[1]);
        lvP2     = TransformPoint(laRows, maVerts[2]);
        lvNormal = TransformDirection(laRows, lvNormal);
    }
    else
    {
        lvP0 = maVerts[0];
        lvP1 = maVerts[1];
        lvP2 = maVerts[2];
    }

    const Vec4 lvE0 = SubXYZW(lvP2, lvP0);   // v9
    const Vec4 lvE1 = SubXYZW(lvP1, lvP2);   // v8
    const Vec4 lvE2 = SubXYZW(lvP0, lvP1);   // v10

    arInst.mVolumeType        = GPInstance::TRIANGLE;   // stw 3, 0x90
    arInst.mNumEdgeDirections = 3;
    arInst.mPos               = lvP0;
    arInst.mNumFaceNormals    = 1;
    arInst.mUserTag           = 0;

    // FLAG (host width): the console stores its 4-byte `this` straight into the
    // u32 mVolumeTag word, and the batch kernels copy that word verbatim into
    // PrimitivePairIntersectResult::v1/v2. The committed GPInstance keeps the
    // field as the console's 32-bit pointer IMAGE, so this truncates on x64;
    // the real fix is to promote GPInstance::mVolumeTag (and PPIR::v1/v2) to a
    // pointer-width field, which belongs to the GPInstance TU, not here.
    // (Verbatim from CapsuleVolume.cpp:177-183 -- the same defect, same site.)
    arInst.mVolumeTag         = static_cast<u32>(reinterpret_cast<uintptr_t>(this));

    // GPTriangle aliasing: verts 1 and 2 ride in the face-normal rows.
    arInst.mFaceNormals[1]    = lvP1;
    arInst.mFaceNormals[2]    = lvP2;

    arInst.mFatness           = mfFatness;
    arInst.mFaceNormals[0]    = lvNormal;

    arInst.mEdgeData[2]       = mafEdgeCos[2];
    arInst.mEdgeData[1]       = mafEdgeCos[1];
    arInst.mEdgeData[0]       = mafEdgeCos[0];

    // Read AFTER GetNormal: the recompute clears bit 0x2 in place, and the asm's
    // lwz 0x5C(r30) sits past the call, so it observes the cleared flags.
    arInst.mFlags             = mFlags;

    // Edge lengths and unit edge directions. No degenerate guard (see above).
    const f32 lfLen0    = std::sqrt(Dot3(lvE0, lvE0));
    const f32 lfLen1    = std::sqrt(Dot3(lvE1, lvE1));
    const f32 lfLen2    = std::sqrt(Dot3(lvE2, lvE2));
    const f32 lfInvLen0 = 1.0f / lfLen0;
    const f32 lfInvLen1 = 1.0f / lfLen1;
    const f32 lfInvLen2 = 1.0f / lfLen2;

    // mDimensions lane w = L0, per the dumped unk_82CDA350 control.
    arInst.mDimensions.x = lfLen0;
    arInst.mDimensions.y = lfLen1;
    arInst.mDimensions.z = lfLen2;
    arInst.mDimensions.w = lfLen0;

    arInst.mEdgeDirections[0].x = lvE0.x * lfInvLen0;
    arInst.mEdgeDirections[0].y = lvE0.y * lfInvLen0;
    arInst.mEdgeDirections[0].z = lvE0.z * lfInvLen0;
    arInst.mEdgeDirections[0].w = lvE0.w * lfInvLen0;

    arInst.mEdgeDirections[1].x = lvE1.x * lfInvLen1;
    arInst.mEdgeDirections[1].y = lvE1.y * lfInvLen1;
    arInst.mEdgeDirections[1].z = lvE1.z * lfInvLen1;
    arInst.mEdgeDirections[1].w = lvE1.w * lfInvLen1;

    arInst.mEdgeDirections[2].x = lvE2.x * lfInvLen2;
    arInst.mEdgeDirections[2].y = lvE2.y * lfInvLen2;
    arInst.mEdgeDirections[2].z = lvE2.z * lfInvLen2;
    arInst.mEdgeDirections[2].w = lvE2.w * lfInvLen2;

    arInst.mMethods = g_aGPVolumeMethods[GPInstance::TRIANGLE];

    return 1;
}

} // namespace collision
} // namespace rw
