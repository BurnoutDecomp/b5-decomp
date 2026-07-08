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
// CreateGPInstance @ 0x82BBAA00 is NOT homed here: it assembles the triangle's
// GP-instance edge-length vector through `vperm ..., v4` with v4 loaded from
// the un-recovered VMX permute-control table unk_82CDA350 (the export carries
// no value for it), and installs the un-homed GPTriangle VolumeMethods table
// (off_82F91920). Without the permute mask the per-lane routing of the packed
// edge data cannot be reconstructed faithfully -- a guessed mask would be
// wrong -- so it is left BLOCKED per the project's no-fabrication rule.
// ===========================================================================

namespace rw
{
namespace collision
{

// unk_8327EF10 -- the GetNormal degenerate-area epsilon (vcmpgtfp guard). No
// value in the export; the role is identical to FeatureEdge's degenerate-edge
// epsilon, so it takes the same inferred magnitude. FLAGGED as inferred.
const f32 TriangleVolume::KF_DEGENERATE_EPSILON = 1.0e-12f;

// dword_8327EEEC -- a module-scope initialisation word Initialize stamps into
// every triangle volume at +0x40. It is a relocated .data value (loaded via
// `lis/addi dword_8327EEE0 ; lwz +0xC`); the export carries no value and none
// of this TU's functions read the field back, so its meaning/owner TU is not
// recovered. It is declared extern so the store is reproduced faithfully
// WITHOUT fabricating a value (the real definition lives in the un-recovered
// GP/volume registration TU). FLAGGED as un-recovered.
extern const u32 g_uTriangleVolumeInitWord;

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

    lpVolume->mInitWord = g_uTriangleVolumeInitWord;

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
