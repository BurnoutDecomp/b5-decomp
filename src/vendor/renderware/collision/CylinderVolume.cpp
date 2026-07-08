#include "vendor/renderware/collision/CylinderVolume.hpp"

#include <cmath>     // sqrt, fabs
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
// Three functions are NOT homed here (see the header for the reasons):
//   CreateGPInstance   @ 0x82BAC7F8  -- un-homed GP VolumeMethods table
//                                       unk_82F918F0[5] (runtime dispatch rodata)
//   FatLineSegIntersect  @ 0x82BAEB10 -- register-level VMX narrow-phase kernel
//   ThinLineSegIntersect @ 0x82BADCE0 -- register-level VMX narrow-phase kernel
// Left BLOCKED per the project's no-fabrication rule.
// ===========================================================================

namespace rw
{
namespace collision
{

// --- un-recovered .rdata Initialize stamps (declared extern; reproduced
// faithfully WITHOUT fabricating values, exactly as TriangleVolume.cpp does for
// its init word). The real definitions live in the un-recovered GP/volume
// registration + math-constant TUs. FLAGGED as un-recovered. ---------------

// dword_8327EEF4 -- module-scope initialisation word stamped at +0x40. Loaded
// via `lis/addi dword_8327EEE0 ; lwz +0x14`; the export carries no value and no
// function in this TU reads the field back.
extern const u32 g_uCylinderVolumeInitWord;

// The default local-frame seed rows. gIVector is w::math::vpu::detail::gIVector
// (an identity basis vector); unk_82181510 / unk_82181520 are the same
// un-recovered axis-seed constants referenced (and flagged) across VehiclePhysics
// / ParticleModule / PropCollisions. They are 16-byte VMX rows.
extern const Vec4 g_vIVector;             // -> maFrame[0]
extern const Vec4 g_vAxisSeed82181510;    // unk_82181510 -> maFrame[1]
extern const Vec4 g_vAxisSeed82181520;    // unk_82181520 -> maFrame[2]

namespace
{
    // vspltisw(-1)/vslw sign-mask abs + the vmulfp/rsqrt fold: |1 - c^2| forced
    // to 0 by the vcmpeqfp/vsel guard, otherwise its true square root.
    inline f32 GuardedSqrt(f32 afValue)
    {
        return (afValue == 0.0f) ? 0.0f : afValue * (1.0f / std::sqrt(afValue));
    }
}

// ---------------------------------------------------------------------------
// CylinderVolume::Initialize @ 0x82BAD3F0
//
//   lwz r11, 0(r3) ; beq -> return 0          ; volume = *appVolume, NULL guard
//   stfs flt_82001CC0(0.0), 0x50              ; mfFatness = 0 (pre-clear)
//   stw  0, 0x54 / 0x58                        ; mu54 / mu58 = 0
//   stw  1, 0x5C                               ; mu5C = 1
//   stw  dword_8327EEF4, 0x40                  ; mInitWord = <module word>
//   stfs f3, 0x50 ; stfs f1, 0x48 ; stfs f2, 0x44   ; fatness / radius / half-h
//   stvx128 gIVector,     r11(0x00)            ; maFrame[0]
//   stvx128 0,            r11(0x30)            ; maFrame[3]
//   stvx128 unk_82181510, r11(0x10)            ; maFrame[1]
//   stvx128 unk_82181520, r11(0x20)            ; maFrame[2]
//   returns the volume pointer.
// ---------------------------------------------------------------------------
CylinderVolume* CylinderVolume::Initialize(CylinderVolume** appVolume,
                                           f32 afRadius, f32 afHalfHeight, f32 afFatness)
{
    CylinderVolume* lpVolume = *appVolume;
    if (lpVolume == nullptr)
    {
        return nullptr;
    }

    lpVolume->mfFatness = 0.0f;   // flt_82001CC0 pre-clear (overwritten below)
    lpVolume->mu54      = 0;
    lpVolume->mu58      = 0;
    lpVolume->mu5C      = 1;
    lpVolume->mInitWord = g_uCylinderVolumeInitWord;

    lpVolume->maFrame[0] = g_vIVector;
    lpVolume->maFrame[3].x = 0.0f;
    lpVolume->maFrame[3].y = 0.0f;
    lpVolume->maFrame[3].z = 0.0f;
    lpVolume->maFrame[3].w = 0.0f;
    lpVolume->maFrame[1] = g_vAxisSeed82181510;
    lpVolume->maFrame[2] = g_vAxisSeed82181520;

    lpVolume->mfFatness    = afFatness;      // f3 -> +0x50
    lpVolume->mfRadius     = afRadius;       // f1 -> +0x48
    lpVolume->mfHalfHeight = afHalfHeight;   // f2 -> +0x44

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
    return math::vpu::Vector3(mfRadius * 2.0f, mfRadius * 2.0f, mfHalfHeight * 2.0f);
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
RwBool CylinderVolume::GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const
{
    const f32 lfRadius  = mfRadius;    // v5 = a1[18] (+0x48)
    const f32 lfFatness = mfFatness;   // v6 = a1[20] (+0x50)

    f32 lfCentreX = 0.0f;
    f32 lfCentreY = 0.0f;
    f32 lfCentreZ = 0.0f;

    f32 lfExtentX;
    f32 lfExtentY;
    f32 lfExtentZ;

    if (lpTransform != nullptr)
    {
        // Fatness folded into both dimensions (fadds f13 / fadds f0).
        const f32 lfAxial  = lfFatness + mfHalfHeight;   // v10 = a1[20]+a1[17]
        const f32 lfRadial = lfFatness + lfRadius;       // fatness + radius

        if (abTight)
        {
            // The cylinder's world axis = the transform's row-2 (lvx128 [r4+0x20]).
            const Vec4& lvAxis = lpTransform[2];

            lfExtentX = std::fabs(lvAxis.x) * lfAxial
                      + GuardedSqrt(std::fabs(1.0f - lvAxis.x * lvAxis.x)) * lfRadial;
            lfExtentY = std::fabs(lvAxis.y) * lfAxial
                      + GuardedSqrt(std::fabs(1.0f - lvAxis.y * lvAxis.y)) * lfRadial;
            lfExtentZ = std::fabs(lvAxis.z) * lfAxial
                      + GuardedSqrt(std::fabs(1.0f - lvAxis.z * lvAxis.z)) * lfRadial;
        }
        else
        {
            // fmadds: lenSq = (fatness+halfHeight)^2 + (fatness+radius)^2, then
            // the guarded sqrt broadcast across every lane.
            const f32 lfLenSq  = lfAxial * lfAxial + lfRadial * lfRadial;
            const f32 lfSphere = GuardedSqrt(lfLenSq);
            lfExtentX = lfSphere;
            lfExtentY = lfSphere;
            lfExtentZ = lfSphere;
        }

        // Centre = the transform translation (row 3, lvx128 [r4+0x30]).
        lfCentreX = lpTransform[3].x;
        lfCentreY = lpTransform[3].y;
        lfCentreZ = lpTransform[3].z;
    }
    else
    {
        // Local axis-aligned box (no orientation): centre stays at the origin.
        lfExtentX = lfRadius + lfFatness;
        lfExtentY = lfRadius + lfFatness;
        lfExtentZ = mfHalfHeight + lfFatness;
    }

    arResult.mMin = math::vpu::Vector3(lfCentreX - lfExtentX,
                                       lfCentreY - lfExtentY,
                                       lfCentreZ - lfExtentZ);
    arResult.mMax = math::vpu::Vector3(lfCentreX + lfExtentX,
                                       lfCentreY + lfExtentY,
                                       lfCentreZ + lfExtentZ);
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
void CylinderVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vec4& /*arDir*/,
                                       Feature& arResult) const
{
    const Vec4& lvAxis   = maFrame[1];
    const Vec4& lvCentre = maFrame[3];
    const f32   lfH      = mfHalfHeight;

    Vec4 lvP0;   // var_70: centre + axis * halfHeight
    lvP0.x = lvAxis.x * lfH + lvCentre.x;
    lvP0.y = lvAxis.y * lfH + lvCentre.y;
    lvP0.z = lvAxis.z * lfH + lvCentre.z;
    lvP0.w = lvAxis.w * lfH + lvCentre.w;

    Vec4 lvP1;   // var_60: centre - axis * halfHeight
    lvP1.x = lvCentre.x - lvAxis.x * lfH;
    lvP1.y = lvCentre.y - lvAxis.y * lfH;
    lvP1.z = lvCentre.z - lvAxis.z * lfH;
    lvP1.w = lvCentre.w - lvAxis.w * lfH;

    const FeatureEdge lEdge(lvP0, lvP1);
    std::memcpy(&arResult.edges[0], &lEdge, sizeof(FeatureEdge));

    arResult.numedges = 1;
}

} // namespace collision
} // namespace rw
