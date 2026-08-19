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

// --- Initialize's +0x40 stamp: NOT a .rdata constant -----------------------
// dword_8327EEF4 -- module-scope word stamped at +0x40, loaded via
// `lis/addi dword_8327EEE0 ; lwz +0x14`.
//
// CORRECTED 2026-08-18 (waveQ5 C1, headless IDA on a private .i64 copy). The
// old comment here called this an "un-recovered .rdata Initialize stamp", i.e.
// a compile-time constant whose value the export happened not to carry. It is
// neither: dword_8327EEE0 is the SEVEN-ENTRY RUNTIME Volume descriptor table
// (rw::collision::gVolumeVTable, defined in SDKs/EATech/rwcollision/volume.cpp
// and filled by rw::collision::Volume::InitializeVTable @ 0x82BB03A8), and
// +0x14 is slot 5 == VOLUMETYPECYLINDER. So the stamp is a RUNTIME READ of
// gVolumeVTable[5] -- the cylinder Volume descriptor POINTER -- exactly the
// value PrimitiveIntersect.cpp's GetVolumeVTable reads back from Volume+0x40.
// The image bytes at 0x8327EEE0..0x8327EEFC are all ZERO (they are .data, not
// .rdata) -- AGENTS gotcha 13: a zero there is "not written yet", not a value.
//
// HOST REPRESENTATION (2026-08-18, wave Q5 integration): the +0x40 slot holds
// the 4-byte VolumeType enum for the record's whole lifetime (an x64 pointer
// would overlap +0x44, and the 96-byte serialised record cannot grow); every
// reader recovers the console pointer as gVolumeVTable[enum]. So Initialize
// stamps E_VOLUMETYPE_CYLINDER (5, the index the console pointer sits at).
// Derivation: scratchpad/waveQ5/rwc3.owner.md section 7 / volume.cpp foot.
static const u32 KU_CYLINDER_VOLUME_TYPE = 5u;   // gVolumeVTable[5] = &unk_82F91894

// --- Initialize's default local-frame seed rows ----------------------------
// RECOVERED 2026-08-18 (waveQ5 C1, headless IDA on a private .i64 copy). These
// were three `extern const Vec4` declarations FLAGGED as "un-recovered axis
// seeds", i.e. three permanent link holes standing in for values that were in
// the image all along. They are the three IDENTITY BASIS ROWS of the SDK's
// I/J/K run, and the .rdata carries them verbatim:
//   0x82181500  w::math::vpu::detail::gIVector  3F800000 00000000 00000000 00000000
//   0x82181510  unk_82181510                    00000000 3F800000 00000000 00000000
//   0x82181520  unk_82181520                    00000000 00000000 3F800000 00000000
// (The IDA symbol is TRUNCATED to `w::math::vpu::detail::gIVector` -- AGENTS
// gotcha 6; the real name is rw::math::vpu::detail::gIVector.)
//
// Spelled as file-scope constants here, not as a new global, for the same
// reason GameSource/Director/Camera/Utils/CameraUtils.cpp:136 spells gIVector
// as a local KV_AXIS_X: the SDK's rw::math::vpu::detail basis run has no home
// TU anywhere in this tree, and inventing `rw::collision::g_vIVector` to
// satisfy three reads would be a second definition of a name that belongs to
// rw::math::vpu::detail (AGENTS gotcha 7). When that home lands, these three
// become references to it.
namespace
{
    const Vec4 KV_BASIS_X = { 1.0f, 0.0f, 0.0f, 0.0f };   // gIVector     @0x82181500
    const Vec4 KV_BASIS_Y = { 0.0f, 1.0f, 0.0f, 0.0f };   // unk_82181510 @0x82181510
    const Vec4 KV_BASIS_Z = { 0.0f, 0.0f, 1.0f, 0.0f };   // unk_82181520 @0x82181520
}

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
    lpVolume->mInitWord = KU_CYLINDER_VOLUME_TYPE;   // console: dword_8327EEF4 = gVolumeVTable[5]

    lpVolume->maFrame[0] = KV_BASIS_X;
    lpVolume->maFrame[3].x = 0.0f;
    lpVolume->maFrame[3].y = 0.0f;
    lpVolume->maFrame[3].z = 0.0f;
    lpVolume->maFrame[3].w = 0.0f;
    lpVolume->maFrame[1] = KV_BASIS_Y;
    lpVolume->maFrame[2] = KV_BASIS_Z;

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
