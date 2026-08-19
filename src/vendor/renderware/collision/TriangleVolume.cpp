#include "vendor/renderware/collision/TriangleVolume.hpp"

#include <cmath>
#include <cstring>   // memcpy (GetMaximumFeature edge copies)

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
const f32 TriangleVolume::KF_DEGENERATE_EPSILON = 1.1920928955078125e-07f;   // 0x34000000, 2^-23

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
// E_VOLUMETYPE_TRIANGLE (3, the index the console pointer sits at) here.
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
TriangleVolume* TriangleVolume::Initialize(TriangleVolume** appVolume,
                                           const Vec4& arP0, const Vec4& arP1, const Vec4& arP2)
{
    TriangleVolume* lpVolume = *appVolume;
    if (lpVolume == nullptr)
    {
        return nullptr;
    }

    lpVolume->maVerts[0] = arP0;
    lpVolume->maVerts[1] = arP1;
    lpVolume->maVerts[2] = arP2;

    lpVolume->mNormal.x = 0.0f;
    lpVolume->mNormal.y = 0.0f;
    lpVolume->mNormal.z = 0.0f;
    lpVolume->mNormal.w = 0.0f;

    lpVolume->mInitWord = KU_TRIANGLE_VOLUME_TYPE;   // console: dword_8327EEEC = gVolumeVTable[3]

    lpVolume->mafEdgeCos[0] = -1.0f;
    lpVolume->mafEdgeCos[1] = -1.0f;
    lpVolume->mafEdgeCos[2] = -1.0f;

    lpVolume->mfFatness = 0.0f;
    lpVolume->mu54      = 0;
    lpVolume->mu58      = 0;
    lpVolume->mFlags    = 0x1E3u;

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
void TriangleVolume::GetNormal(Vec4& arNormal, const Vec4* lpTransform) const
{
    Vec4 lNormal = mNormal;

    if ((mFlags & 0x2u) != 0)
    {
        const Vec4 lE2 = SubXYZW(maVerts[2], maVerts[0]);
        const Vec4 lE1 = SubXYZW(maVerts[1], maVerts[0]);

        mFlags &= ~0x2u;

        lNormal = Cross3(lE1, lE2);

        // vcmpgtfp guard + vsel: normalise only when the area is non-degenerate,
        // otherwise keep the raw (unnormalised) cross product.
        const f32 lfLenSq = Dot3(lNormal, lNormal);
        if (lfLenSq > KF_DEGENERATE_EPSILON)
        {
            const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);
            lNormal.x *= lfInvLen;
            lNormal.y *= lfInvLen;
            lNormal.z *= lfInvLen;
            lNormal.w *= lfInvLen;
        }

        mNormal = lNormal;
    }

    if (lpTransform != nullptr)
    {
        lNormal = TransformDirection(lpTransform, lNormal);
    }

    arNormal = lNormal;
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetPoints @ 0x82BB11F8
//
// lpTransform == NULL: copy the local vertices straight through.
// lpTransform != NULL: affine-transform each vertex by the four rows.
// ---------------------------------------------------------------------------
void TriangleVolume::GetPoints(Vec4& arP0, Vec4& arP1, Vec4& arP2, const Vec4* lpTransform) const
{
    if (lpTransform != nullptr)
    {
        arP0 = TransformPoint(lpTransform, maVerts[0]);
        arP1 = TransformPoint(lpTransform, maVerts[1]);
        arP2 = TransformPoint(lpTransform, maVerts[2]);
    }
    else
    {
        arP0 = maVerts[0];
        arP1 = maVerts[1];
        arP2 = maVerts[2];
    }
}

// ---------------------------------------------------------------------------
// TriangleVolume::GetBBox @ 0x82BBA038
//
// Optionally transform the three vertices, min/max them (vminfp/vmaxfp), then
// fatten by mfFatness (this+0x50, broadcast): min -= fatness, max += fatness.
// Stores min @ result+0x00, max @ result+0x10. abTight (r5) is unused. Returns 1.
// ---------------------------------------------------------------------------
RwBool TriangleVolume::GetBBox(const Vec4* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    Vec4 lV0;
    Vec4 lV1;
    Vec4 lV2;

    if (lpTransform != nullptr)
    {
        lV0 = TransformPoint(lpTransform, maVerts[0]);
        lV1 = TransformPoint(lpTransform, maVerts[1]);
        lV2 = TransformPoint(lpTransform, maVerts[2]);
    }
    else
    {
        lV0 = maVerts[0];
        lV1 = maVerts[1];
        lV2 = maVerts[2];
    }

    // vminfp v11,v13,v12 ; vminfp v13,v0,v11  (and the vmaxfp mirror).
    Vec4 lMin = MinXYZW(lV0, MinXYZW(lV1, lV2));
    Vec4 lMax = MaxXYZW(lV0, MaxXYZW(lV1, lV2));

    // lvlx this+0x50 ; vspltw lane0 : the fatness broadcast.
    const f32 lfFatness = mfFatness;
    lMin.x -= lfFatness; lMin.y -= lfFatness; lMin.z -= lfFatness;
    lMax.x += lfFatness; lMax.y += lfFatness; lMax.z += lfFatness;

    arResult.mMin = math::vpu::Vector3(lMin.x, lMin.y, lMin.z);
    arResult.mMax = math::vpu::Vector3(lMax.x, lMax.y, lMax.z);

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
    // vminfp v10,v12,v13 ; vminfp v12,v0,v10  (and the vmaxfp mirror) -- the
    // same fold order GetBBox uses.
    const Vec4 lLo = MinXYZW(maVerts[0], MinXYZW(maVerts[1], maVerts[2]));
    const Vec4 lHi = MaxXYZW(maVerts[0], MaxXYZW(maVerts[1], maVerts[2]));

    // lvlx this+0x50 ; vspltw lane0 : the fatness broadcast.
    const f32 lfFatness = mfFatness;

    const f32 lfDiagX = (lHi.x + lfFatness) - (lLo.x - lfFatness);
    const f32 lfDiagY = (lHi.y + lfFatness) - (lLo.y - lfFatness);
    const f32 lfDiagZ = (lHi.z + lfFatness) - (lLo.z - lfFatness);

    return math::vpu::Vector3(lfDiagX, lfDiagY, lfDiagZ);
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
void TriangleVolume::GetMaximumFeature(const Vec4& /*arDir*/, Feature& arResult) const
{
    const FeatureEdge lEdge0(maVerts[0], maVerts[1]);
    std::memcpy(&arResult.edges[0], &lEdge0, sizeof(FeatureEdge));

    const FeatureEdge lEdge1(maVerts[1], maVerts[2]);
    std::memcpy(&arResult.edges[1], &lEdge1, sizeof(FeatureEdge));

    const FeatureEdge lEdge2(maVerts[2], maVerts[0]);
    std::memcpy(&arResult.edges[2], &lEdge2, sizeof(FeatureEdge));

    arResult.numedges = 3;
}

// ---------------------------------------------------------------------------
// TriangleVolume::LineSegIntersect @ 0x82BBB970
//
// Fetch the three (optionally transformed) triangle points via GetPoints, then
// dispatch to TriangleLineSegIntersect with the segment start (v1), the segment
// delta (aLineEnd - aLineStart) and the fatness. Returns its hit code.
// ---------------------------------------------------------------------------
s32 TriangleVolume::LineSegIntersect(const Vec4* lpTransform,
                                     VolumeLineSegIntersectResult* lpResult,
                                     f32 afFatness, Vec4 aLineStart, Vec4 aLineEnd) const
{
    Vec4 lP0;
    Vec4 lP1;
    Vec4 lP2;
    GetPoints(lP0, lP1, lP2, lpTransform);

    const Vec4 lLineDelta = SubXYZW(aLineEnd, aLineStart);

    return TriangleLineSegIntersect(lpResult, aLineStart, lLineDelta, lP0, lP1, lP2, afFatness);
}

} // namespace collision
} // namespace rw
