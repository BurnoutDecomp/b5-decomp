#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "rw/rwcore_structs.h"
#include <cfloat>

#include <cmath>
#include <cstring>   // memcpy (GetMaximumFeature edge copies)

// includes folded in from the TriangleVolume_w*.cpp partfiles (2026-09-15)
#include "vendor/renderware/collision/GPInstance.hpp"   // GPInstance (CreateGPInstance)
#include "types.hpp"   // uintptr_t (the console 32-bit mVolumeTag pointer image)

// ===========================================================================
// rw::collision::TriangleVolume -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following this directory's
// precedent (FeatureEdge.cpp / GPSphere.cpp), these are SEMANTIC reconstructions
// lowered to portable scalar float maths, preserving the store ORDER, offsets
// and every side effect. The VMX reciprocal-sqrt / reciprocal refinement
// idioms (vrsqrtefp/vrefp + Newton-Raphson) de-optimise to the exact real
// operations they approximate (sqrt / divide); see FeatureEdge.cpp for the same
// treatment.
//
// CreateGPInstance @ 0x82BBAA00 is HOMED, in the sibling partfile
// TriangleVolume_wN_01.cpp (mounted at build_game_exe.bat:2088). CORRECTED
// 2026-08-19 (wave Q6 cluster C4): this banner still said "not yet homed here
// ... Implementation pending", which is a stale claim in the helpful direction
// (AGENTS gotcha 10) -- the body landed and the descriptor's createGPInstance
// slot for TRIANGLE has been bound to it since wave Q5. The evidence that
// unblocked it is kept because it is still the reading key for that body:
// unk_82CDA350 is DUMPED -- bytes 00 01 02 03 | 14 15 16 17 | 00 01 02 03 |
// 00 01 02 03, i.e. vperm(vA, vB, ctl) = (vA.x, vB.y, vA.x, vA.x) -- and
// off_82F91920 is row [3] (TRIANGLE) of the recovered unk_82F918F0
// VolumeMethods table (0x82F918F0 + 3*0x10), i.e.
// g_aGPVolumeMethods[GPInstance::TRIANGLE]. Full recovered tables +
// per-instruction walk-through: scratchpad/waveN/TriangleVolume.spec.md.
//
// GetBBoxDiag @ 0x82BBA110 IS homed here as of 2026-08-19 (wave Q6 cluster C4).
// It was an EXPORT HOLE -- no per-address JSON, no identity.json row -- and was
// closed by a targeted headless idat run on a PRIVATE .i64 copy;
// scratchpad/waveQ6/ida_vt2/asm/0x82BBA110.txt is the raw dump the body below
// is read from.
// ===========================================================================

namespace rw
{
namespace collision
{

// unk_8327EF10 -- the GetNormal degenerate-area epsilon (vcmpgtfp guard).
// MEASURED (2026-08-04 unblock dump): unk_8327EF10 sits in writable .data and
// is filled at static-init time by the splat thunk @ 0x82C73C70
// (lvlx unk_82180554 ; vspltw 0 ; stvx128 -> 0x8327EF10); the .rdata source
// word 0x82180554 is 0x34000000 = 2^-23 = FLT_EPSILON. The earlier 1.0e-12f
// here was an inference and is superseded by this measured value.
const f32 TriangleVolume::KF_DEGENERATE_EPSILON = 1.1920928955078125e-07f;
// ARTIST 82C73C70 initializes this volume.h:37 TU-local VecFloat.
const rw::math::vpu::VecFloat VEC_EPSILON(FLT_EPSILON);   // 0x34000000, 2^-23

// dword_8327EEEC -- RECOVERED (2026-08-04 unblock dump): entry [3] (TRIANGLE)
// of the per-VolumeType vtable registry at dword_8327EEE0, runtime-written by
// rw::collision::Volume::InitializeVTable @ 0x82BB03A8:
//   [0]=0, [1]=&unk_82F91740 (SPHERE),  [2]=&unk_82F918C0 (CAPSULE),
//   [3]=&unk_82F919A4 (TRIANGLE),      [4]=&unk_82F9176C (BOX),
//   [5]=&unk_82F91894 (CYLINDER),      [6]=&unk_82F919D0 (AGGREGATE).
// So the word Initialize stamps at +0x40 is the console 32-bit POINTER IMAGE
// of the triangle Volume vtable record (the block PrimitiveIntersect.cpp's
// GetVolumeVTable reads back from Volume+0x40; its +0x14 slot is this class's
// own CreateGPInstance @ 0x82BBAA00).
// HOST REPRESENTATION (2026-08-18, wave Q5 integration): the +0x40 slot holds
// the 4-byte VolumeType enum for the record's whole lifetime (an x64 pointer
// would overlap +0x44, and the 96-byte serialised record cannot grow); every
// reader recovers the pointer as gVolumeVTable[enum]. So Initialize stamps
// VOLUMETYPETRIANGLE (3, the index the console pointer sits at) here.
// Derivation: scratchpad/waveQ5/rwc3.owner.md section 7 / volume.cpp foot.
static const u32 KU_TRIANGLE_VOLUME_TYPE = 3u;   // gVolumeVTable[3] = &unk_82F919A4

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128 / lane-broadcast fold).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // e1 x e2 over the xyz lanes (the GetNormal permute-cross idiom
    // vpermwi128 0x63 == YZX swizzle -> the standard cross product).
    inline Vec4 Cross3(const Vec4& e1, const Vec4& e2)
    {
        Vec4 r;
        r.x = e1.y * e2.z - e1.z * e2.y;
        r.y = e1.z * e2.x - e1.x * e2.z;
        r.z = e1.x * e2.y - e1.y * e2.x;
        r.w = 0.0f;
        return r;
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

    inline Vec4 MinXYZW(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x < b.x ? a.x : b.x;
        r.y = a.y < b.y ? a.y : b.y;
        r.z = a.z < b.z ? a.z : b.z;
        r.w = a.w < b.w ? a.w : b.w;
        return r;
    }

    inline Vec4 MaxXYZW(const Vec4& a, const Vec4& b)
    {
        Vec4 r;
        r.x = a.x > b.x ? a.x : b.x;
        r.y = a.y > b.y ? a.y : b.y;
        r.z = a.z > b.z ? a.z : b.z;
        r.w = a.w > b.w ? a.w : b.w;
        return r;
    }
}

// ---------------------------------------------------------------------------
// TriangleVolume::Initialize @ 0x82BB0680
//
//   lwz r11, 0(r3) ; beq -> return 0     ; volume = *appVolume, NULL guard
//   stvx128 v1, r11(0x00)                ; maVerts[0] = arP0
//   stvx128 v2, r11(0x10)                ; maVerts[1] = arP1
//   stvx128 v3, r11(0x20)                ; maVerts[2] = arP2
//   stvx128 v0(=0), r11(0x30)            ; mNormal = 0
//   stw dword_8327EEEC, 0x40(r11)        ; mInitWord = <module word>
//   stfs flt_820037C8(-1.0), 0x44/48/4C  ; mafEdgeCos = -1
//   stfs flt_82001CC0( 0.0), 0x50        ; mfFatness = 0
//   stw  0, 0x54 / 0x58                  ; mu54 / mu58 = 0
//   stw  0x1E3, 0x5C                     ; mFlags = 0x1E3
//   returns the volume pointer.
// ---------------------------------------------------------------------------
TriangleVolume* TriangleVolume::Initialize(const ::rw::Resource& arResource,
                                           Vector3 arP0, Vector3 arP1, Vector3 arP2)
{
    TriangleVolume* lpVolume = static_cast<TriangleVolume*>(arResource.m_baseResources[0]);
    if (!lpVolume) return 0;
    lpVolume->mfRadius = 0.0f;
    lpVolume->muGroupID = 0;
    lpVolume->mTriangleData.mfEdgeCos0 = -1.0f;
    lpVolume->muSurfaceID = 0;
    lpVolume->mTriangleData.mfEdgeCos1 = -1.0f;
    lpVolume->mTriangleData.mfEdgeCos2 = -1.0f;
    lpVolume->mpVTable = gVolumeVTable[VOLUMETYPETRIANGLE];
    lpVolume->mTransform.xAxis = arP0;
    lpVolume->muFlags = 0x1e3;
    lpVolume->mTransform.yAxis = arP1;
    lpVolume->mTransform.zAxis = arP2;
    lpVolume->mTransform.wAxis.mV = __vspltisw(0);
    return lpVolume;
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetNormal @ 0x82BB0580
//
// v0 = mNormal (the cached local normal). If the mFlags 0x2 "dirty" bit is set:
//   e2 = maVerts[2] - maVerts[0]                (vsubfp)
//   e1 = maVerts[1] - maVerts[0]                (vsubfp)
//   clear mFlags 0x2
//   n  = e1 x e2                                (vpermwi 0x63 cross idiom)
//   if ( dot3(n,n) > epsilon )  n *= 1/sqrt(dot3(n,n))   (vrsqrtefp+NR, vsel)
//   mNormal = n
// n = mNormal
// if ( lpTransform )  n = 3x3(lpTransform) * n           (vspltw + vmaddfp)
// *arNormal = n.
// ---------------------------------------------------------------------------
void TriangleVolume::GetNormal(Vector3& arNormal, const Matrix44Affine* lpTransform) const
{
    __vector4 normal = mTransform.wAxis.mV;
    if (muFlags & 2u)
    {
        const __vector4 p0 = mTransform.xAxis.mV;
        const __vector4 e2 = __vsubfp(mTransform.zAxis.mV, p0);
        const __vector4 e1 = __vsubfp(mTransform.yAxis.mV, p0);
        muFlags &= ~2u;
        const __vector4 perm2 = __vpermwi(e2, 0x63);
        const __vector4 perm1 = __vpermwi(e1, 0x63);
        const __vector4 product = __vmulfp(e1, perm2);
        const __vector4 cross = __vpermwi(__vnmsubfp(perm1, e2, product), 0x63);
        const __vector4 square = __vmsum3fp(cross, cross);
        const __vector4 valid = __vcmpgtfp(square, VEC_EPSILON.mV);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 estimate = __vrsqrtefp(square);
        const __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        const __vector4 inverse = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        normal = __vsel(cross, __vmulfp(cross, inverse), valid);
        mTransform.wAxis.mV = normal;
    }
    if (lpTransform)
    {
        __vector4 rotated = __vmulfp(lpTransform->xAxis.mV, __vspltw(normal, 0));
        rotated = __vmaddfp(lpTransform->yAxis.mV, __vspltw(normal, 1), rotated);
        normal = __vmaddfp(lpTransform->zAxis.mV, __vspltw(normal, 2), rotated);
    }
    arNormal.mV = normal;
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetPoints @ 0x82BB11F8
//
// lpTransform == NULL: copy the local vertices straight through.
// lpTransform != NULL: affine-transform each vertex by the four rows.
// ---------------------------------------------------------------------------
static __vector4 TriangleTransformPoint(__vector4 point, const Matrix44Affine& transform)
{
    __vector4 result = __vmaddfp(transform.xAxis.mV, __vspltw(point, 0), transform.wAxis.mV);
    result = __vmaddfp(transform.yAxis.mV, __vspltw(point, 1), result);
    return __vmaddfp(transform.zAxis.mV, __vspltw(point, 2), result);
}

void TriangleVolume::GetPoints(Vector3& arP0, Vector3& arP1, Vector3& arP2, const Matrix44Affine* lpTransform) const
{
    if (lpTransform)
    {
        arP0.mV = TriangleTransformPoint(mTransform.xAxis.mV, *lpTransform);
        arP1.mV = TriangleTransformPoint(mTransform.yAxis.mV, *lpTransform);
        arP2.mV = TriangleTransformPoint(mTransform.zAxis.mV, *lpTransform);
    }
    else
    {
        arP0 = mTransform.xAxis;
        arP1 = mTransform.yAxis;
        arP2 = mTransform.zAxis;
    }
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetBBox @ 0x82BBA038
//
// Optionally transform the three vertices, min/max them (vminfp/vmaxfp), then
// fatten by mfFatness (this+0x50, broadcast): min -= fatness, max += fatness.
// Stores min @ result+0x00, max @ result+0x10. abTight (r5) is unused. Returns 1.
// ---------------------------------------------------------------------------
RwBool TriangleVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 p0 = mTransform.xAxis.mV;
    __vector4 p1 = mTransform.yAxis.mV;
    __vector4 p2 = mTransform.zAxis.mV;
    if (lpTransform)
    {
        p0 = TriangleTransformPoint(p0, *lpTransform);
        p1 = TriangleTransformPoint(p1, *lpTransform);
        p2 = TriangleTransformPoint(p2, *lpTransform);
    }
    const __vector4 low = __vminfp(p0, __vminfp(p1, p2));
    const __vector4 high = __vmaxfp(p0, __vmaxfp(p1, p2));
    arResult.mMin.mV = low;
    arResult.mMax.mV = high;
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    arResult.mMin.mV = __vsubfp(low, radius);
    arResult.mMax.mV = __vaddfp(arResult.mMax.mV, radius);
    return 1;
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetBBoxDiag @ 0x82BBA110  (18 instructions, no callees)
//
// RECOVERED 2026-08-19 (wave Q6 cluster C4). This address had NO per-address
// export JSON and no progress/identity.json row -- it was an EXPORT HOLE, which
// is exactly why the wave-Q5 descriptor binding had to leave the TRIANGLE
// getBBoxDiag slot NULL. Closed by a targeted headless idat run on a PRIVATE
// copy of the .i64 (scratchpad/waveQ6/ida_vt2/dump_vt2.py -> out.json), so the
// body below comes from the same raw asm every other body in this TU does, not
// from an .i64 label (AGENTS gotcha 6).
//
//   li      r11, 0x20
//   lvx128  v0,  r0, r4        ; maVerts[0]
//   li      r10, 0x10
//   li      r9,  0x50
//   lvx128  v13, r4, r11       ; maVerts[2]
//   lvx128  v12, r4, r10       ; maVerts[1]
//   vminfp  v10, v12, v13      ; min(v1, v2)
//   lvlx    v11, r4, r9        ; the +0x50 quadword, left-justified...
//   vmaxfp  v13, v12, v13      ; max(v1, v2)
//   vspltw  v11, v11, 0        ; ...lane 0 == mfFatness, broadcast
//   vminfp  v12, v0,  v10      ; lo = min(v0, min(v1, v2))
//   vmaxfp  v0,  v0,  v13      ; hi = max(v0, max(v1, v2))
//   vsubfp  v13, v12, v11      ; fattened min = lo - fatness
//   vaddfp  v0,  v0,  v11      ; fattened max = hi + fatness
//   vsubfp  v0,  v0,  v13      ; diagonal     = fattened max - fattened min
//   stvx128 v0,  r0, r3        ; -> the hidden 16-byte return slot
//   blr
//
// So the diagonal is exactly the extent of the SAME fattened box GetBBox
// @0x82BBA038 builds -- with the local vertices only. Unlike GetBBox there is
// no transform arm at all: the console reads r4 (this) and nothing else, which
// matches DWARF triangle.h:119 taking no parameters.
//
// The three siblings that already had this slot bound compute the same quantity
// the same way (CylinderVolume::GetBBoxDiag @0x82BAC7B8 doubles the local
// half-extent, CapsuleVolume::GetBBoxDiag @0x82BAF6A8 doubles |axis|*h + r).
// Note the algebra does NOT simplify to `hi - lo + 2*fatness` on the console:
// the two vsubfp/vaddfp round trips are performed in float, and reproducing the
// operation ORDER is what keeps the result bit-identical.
// ---------------------------------------------------------------------------
math::vpu::Vector3 TriangleVolume::GetBBoxDiag() const
{
    const __vector4 p0 = mTransform.xAxis.mV;
    const __vector4 p1 = mTransform.yAxis.mV;
    const __vector4 p2 = mTransform.zAxis.mV;
    const __vector4 low = __vminfp(p0, __vminfp(p1, p2));
    const __vector4 high = __vmaxfp(p0, __vmaxfp(p1, p2));
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    return Vector3(__vsubfp(__vaddfp(high, radius), __vsubfp(low, radius)));
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetMaximumFeature @ 0x82BBACD0
//
// Build the three boundary edges of the triangle into the result Feature's
// edges[0..2] and set its edge count to 3 (result+0x230). The asm loads the
// local vertices into three stack rows, constructs one FeatureEdge per edge
// (V0V1, V1V2, V2V0) and memcpy's each 0x40-byte record into edges[i]. arDir
// (r4) is unused -- the triangle's whole face is its maximum feature.
// ---------------------------------------------------------------------------
RwBool TriangleVolume::GetMaximumFeature(RwBool /*abCcw*/, Vector3 /*arDir*/, Feature& arResult) const
{
    const Vector3 p0 = mTransform.xAxis;
    const Vector3 p1 = mTransform.yAxis;
    const Vector3 p2 = mTransform.zAxis;
    const FeatureEdge edge0(p0, p1);
    memcpy(&arResult.edges[0], &edge0, sizeof(edge0));
    const FeatureEdge edge1(p1, p2);
    memcpy(&arResult.edges[1], &edge1, sizeof(edge1));
    const FeatureEdge edge2(p2, p0);
    memcpy(&arResult.edges[2], &edge2, sizeof(edge2));
    arResult.numedges = 3;
    return 1;
}

// ---------------------------------------------------------------------------
// TriangleVolume::LineSegIntersect @ 0x82BBB970
//
// Fetch the three (optionally transformed) triangle points via GetPoints, then
// dispatch to TriangleLineSegIntersect with the segment start (v1), the segment
// delta (aLineEnd - aLineStart) and the fatness. Returns its hit code.
// ---------------------------------------------------------------------------
RwBool TriangleVolume::LineSegIntersect(Vector3 aLineStart, Vector3 aLineEnd,
                                        const Matrix44Affine* lpTransform,
                                        VolumeLineSegIntersectResult& lrResult, f32 afFatness) const
{
    Vector3 p0, p1, p2;
    GetPoints(p0, p1, p2, lpTransform);
    const Vector3 delta(__vsubfp(aLineEnd.mV, aLineStart.mV));
    return TriangleLineSegIntersect(&lrResult, aLineStart, delta, p0, p1, p2, afFatness);
}

} // namespace collision
} // namespace rw

// ============================================================================
// FOLDED FROM TriangleVolume_wN_01.cpp (wave N) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

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

// (fold: an identical definition of Dot3 was dropped here -- this TU defines it once, above)

// (fold: an identical definition of TransformPoint was dropped here -- this TU defines it once, above)

// (fold: an identical definition of TransformDirection was dropped here -- this TU defines it once, above)

// (fold: an identical definition of SubXYZW was dropped here -- this TU defines it once, above)
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
static __vector4 TriangleInverseLength(__vector4 delta)
{
    const __vector4 square = __vmsum3fp(delta, delta);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 estimate = __vrsqrtefp(square);
    const __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
    return __vmaddfp(__vmulfp(estimate, half), correction, estimate);
}

static __vector4 TriangleLengthFromInverse(__vector4 inverse)
{
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    __vector4 estimate = __vrefp(inverse);
    __vector4 correction = __vnmsubfp(estimate, inverse, one);
    estimate = __vmaddfp(estimate, correction, estimate);
    correction = __vnmsubfp(estimate, inverse, one);
    return __vmaddfp(estimate, correction, estimate);
}

RwBool TriangleVolume::CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const
{
    __vector4 p0 = mTransform.xAxis.mV;
    __vector4 p1 = mTransform.yAxis.mV;
    __vector4 p2 = mTransform.zAxis.mV;
    Vector3 localNormal;
    GetNormal(localNormal, 0);
    __vector4 normal = localNormal.mV;
    if (lpTransform)
    {
        const __vector4 zero = __vspltisw(0);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 tx = __vrlimi(lpTransform->xAxis.mV, zero, 1, 0);
        const __vector4 ty = __vrlimi(lpTransform->yAxis.mV, zero, 1, 0);
        const __vector4 tz = __vrlimi(lpTransform->zAxis.mV, zero, 1, 0);
        const __vector4 tw = __vrlimi(lpTransform->wAxis.mV, one, 1, 0);
        __vector4 a = __vmaddfp(tx, __vspltw(p0, 0), tw);
        __vector4 b = __vmaddfp(tx, __vspltw(p1, 0), tw);
        __vector4 c = __vmaddfp(tx, __vspltw(p2, 0), tw);
        __vector4 n = __vmulfp(tx, __vspltw(normal, 0));
        a = __vmaddfp(ty, __vspltw(p0, 1), a);
        b = __vmaddfp(ty, __vspltw(p1, 1), b);
        c = __vmaddfp(ty, __vspltw(p2, 1), c);
        n = __vmaddfp(ty, __vspltw(normal, 1), n);
        p0 = __vmaddfp(tz, __vspltw(p0, 2), a);
        p1 = __vmaddfp(tz, __vspltw(p1, 2), b);
        p2 = __vmaddfp(tz, __vspltw(p2, 2), c);
        normal = __vmaddfp(tz, __vspltw(normal, 2), n);
    }
    const __vector4 e0 = __vsubfp(p2, p0);
    const __vector4 e1 = __vsubfp(p1, p2);
    const __vector4 e2 = __vsubfp(p0, p1);
    const __vector4 i0 = TriangleInverseLength(e0);
    const __vector4 i1 = TriangleInverseLength(e1);
    const __vector4 i2 = TriangleInverseLength(e2);
    const __vector4 l0 = TriangleLengthFromInverse(i0);
    const __vector4 l1 = TriangleLengthFromInverse(i1);
    const __vector4 l2 = TriangleLengthFromInverse(i2);
    __vector4 dimensions = __vrlimi(l0, l1, 4, 0);
    dimensions = __vrlimi(dimensions, l2, 2, 0);
    arInst.mVolumeType = GPInstance::TRIANGLE;
    arInst.mNumEdgeDirections = 3;
    arInst.mPos.mV = p0;
    arInst.mNumFaceNormals = 1;
    arInst.mUserTag = 0;
    arInst.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mFaceNormals[1].mV = p1;
    arInst.mFaceNormals[2].mV = p2;
    arInst.mFatness = mfRadius;
    arInst.mFaceNormals[0].mV = normal;
    arInst.mEdgeData[2] = mTriangleData.mfEdgeCos2;
    arInst.mEdgeData[1] = mTriangleData.mfEdgeCos1;
    arInst.mEdgeData[0] = mTriangleData.mfEdgeCos0;
    arInst.mFlags = muFlags;
    arInst.mDimensions.mV = dimensions;
    arInst.mEdgeDirections[0].mV = __vmulfp(e0, i0);
    arInst.mEdgeDirections[1].mV = __vmulfp(e1, i1);
    arInst.mEdgeDirections[2].mV = __vmulfp(e2, i2);
    arInst.mMethods = g_aGPVolumeMethods[GPInstance::TRIANGLE];
    return 1;
}

} // namespace collision
} // namespace rw

// ARTIST folds these genuine per-type methods to 82BAC980, 82B0F1B8 or 82AD5078.
namespace rw { namespace collision {
RwBool TriangleVolume::GetInterval(Vector3, Interval&) const { return 1; }
void TriangleVolume::Release() {}
} }
