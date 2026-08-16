#ifndef BRN_GLOBAL_IRRADIANCE_MANAGER_H
#define BRN_GLOBAL_IRRADIANCE_MANAGER_H

#include <cstddef>            // offsetof (the layout pins below)

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3 / Matrix44 (== rw::math::vpu, 16-byte lanes, 64-byte matrix)

// ============================================================================
// GameSource/World/EnvironmentManager/BrnGlobalIrradianceManager.h
//
// BrnWorld::GlobalIrradianceManager -- the world's ambient-light solver. It takes the
// blended environment keyframe's SIX authored fill colours (already scaled by
// LightingData::mfAmbientIrradianceScale by the caller) plus the key-light direction,
// projects them onto the nine order-2 spherical-harmonic basis functions, and folds the
// result into three 4x4 "irradiance matrices" -- one per colour channel -- such that
//
//     E_channel(n) = [n.x n.y n.z 1] * M_channel * [n.x n.y n.z 1]^T
//
// is the irradiance arriving at a surface whose normal is the unit vector n. That is the
// Ramamoorthi & Hanrahan "Efficient Representation for Irradiance Environment Maps"
// (SIGGRAPH 2001) formulation; the twelve `KF_*` constants in the .cpp are its published
// basis normalisers and matrix coefficients, and their VALUES are proven in the .cpp
// banner against the shipped noon keyframe.
//
// LAYOUT -- DWARF references/DecFIGS/dwarfdump/GameSource/World/EnvironmentManager/
// BrnGlobalIrradianceManager.h:90-92 supplies the names/types; the X360 asm supplies the
// offsets, and the containing EnvironmentManager header pins the 0x160 total extent
// (mGlobalIrradianceManager@0x700 vs miToolUpdateFrameCounter@0x860):
//     +0x000  Matrix44 maIrradianceMatrix[3]     (GetIrradianceMatrix @0x827B0500 indexes
//                                                 it with `colour << 6`; ComputeIrradiance
//                                                 @0x827CA978 passes +0x00/+0x40/+0x80)
//     +0x0C0  Vector3  mAverageIrradianceColour  (ComputeIrradiance's final stvx128 at
//                                                 r31+0xC0; read inlined by
//                                                 GenerateShaderConstants @0x827D06D0 as
//                                                 EnvironmentManager+0x7C0)
//     +0x0D0  Vector3  maFrameIrradianceCoeffs[9] (ComputeFrameCoeffs memsets +0xD0..+0x160;
//                                                 UpdateCoefficients @0x827BEF70 writes
//                                                 +0xD0,+0xE0,...,+0x150)
//
// NOT RECONSTRUCTED (and why -- see the report): the DWARF also declares Construct (:39),
// Prepare (:43) and SampleIrradiance (:64). None of the three exists in the X360 build:
// there is no exported body for them, and EnvironmentManager::Construct @0x827CA408 makes
// no store anywhere in [+0x700, +0x860) and calls nothing in this class -- so on ARTIST the
// irradiance manager is never explicitly initialised (GenerateShaderConstants always runs
// ComputeIrradiance before any GetIrradianceMatrix, so nothing reads stale state). Per the
// "DWARF supplies names; the X360 ledger decides what exists" rule they are left out rather
// than invented.
// ============================================================================

namespace BrnWorld
{
class GlobalIrradianceManager
{
public:
    // The three colour channels (R, G, B) the irradiance matrices are indexed by.
    // ComputeIrradiance drives ComputeIrradianceMatrix with 0/1/2 and GetIrradianceMatrix
    // asserts `< 3`.
    enum { E_NUM_COLOURS = 3 };

    // The nine order-2 spherical-harmonic coefficients (l = 0..2). INFERRED name: the
    // count is attested (the memset of 0x90 bytes = 9 * 16, and the nine distinct store
    // offsets +0xD0..+0x150), the identifier is not.
    enum { E_NUM_COEFFS = 9 };

    // ComputeIrradiance @ 0x827CA978. DWARF :54.
    // Runs ComputeFrameCoeffs over the six fills, then builds one irradiance matrix per
    // colour channel and caches the constant (n-independent) term as the average colour.
    void ComputeIrradiance( const Vector3& lKeyLightDirection,
                            const Vector3& lSunFillColour,
                            const Vector3& lShadowFillColour,
                            const Vector3& lRightFillColour,
                            const Vector3& lLeftFillColour,
                            const Vector3& lTopFillColour,
                            const Vector3& lBottomFillColour );

    // GetIrradianceMatrix @ 0x827B0500. DWARF :57 declares it `Matrix44 GetIrradianceMatrix(uint8_t) const`
    // (by value); the X360 asm passes the sret buffer in r3, `this` in r4 and the channel in r5 and
    // returns the buffer pointer, which is what this signature models. KEPT VERBATIM from the
    // committed reconstruction (only the member it reads was renamed to the DWARF spelling).
    Matrix44& GetIrradianceMatrix( Matrix44& lrOutMatrix, u8 lu8Colour ) const;

    // GetAverageIrradianceColour. DWARF :60. INLINED by the X360 compiler -- it has no
    // exported body; the read is recovered from its only caller,
    // EnvironmentManager::GenerateShaderConstants @0x827D0098, whose
    // `li r6, 0x7C0 ; lvx128 v0, r31, r6` loads EnvironmentManager+0x7C0, i.e. this+0xC0.
    const Vector3& GetAverageIrradianceColour() const { return mAverageIrradianceColour; }

protected:
    // UpdateCoefficients @ 0x827BEF70. DWARF :71.
    // Accumulates one directional sample into the nine SH coefficients:
    //   maFrameIrradianceCoeffs[i] += lColour * Y_i(lVector)
    // lVector is the direction the light arrives FROM (see the .cpp banner for the proof).
    void UpdateCoefficients( const Vector3& lColour, const Vector3& lVector );

    // ComputeIrradianceMatrix @ 0x827B1160. DWARF :77.
    // Builds the Ramamoorthi irradiance matrix for one colour channel out of the nine
    // coefficients. (`this` is unused -- the X360 body overwrites r3 in its first
    // instruction after the prologue -- but the DWARF declares it a member, so it stays one.)
    // The coefficient pointer is non-const to match the DWARF declaration exactly even though
    // the body only reads through it; the DWARF's .cpp rendering of lMatrixOut as
    // `const Matrix44 &` is a dwarfdump artefact -- every one of its sixteen elements is stored.
    void ComputeIrradianceMatrix( s32 liChannel, Vector3* lpIrradianceCoeffs,
                                  Matrix44& lrMatrixOut );

    // ComputeFrameCoeffs @ 0x827C5188. DWARF :87.
    // Builds the three-axis fill rig from the key-light direction and projects the six fill
    // colours onto the SH basis (clearing the coefficients first).
    void ComputeFrameCoeffs( const Vector3& lKeyLightDirection,
                             const Vector3& lSunFillColour,
                             const Vector3& lShadowFillColour,
                             const Vector3& lRightFillColour,
                             const Vector3& lLeftFillColour,
                             const Vector3& lUpFillColour,
                             const Vector3& lDownFillColour );

protected:
    Matrix44 maIrradianceMatrix[E_NUM_COLOURS];      // +0x000  DWARF :90
    Vector3  mAverageIrradianceColour;               // +0x0C0  DWARF :91
    Vector3  maFrameIrradianceCoeffs[E_NUM_COEFFS];  // +0x0D0  DWARF :92

public:
    // Never called; pins the offsets the X360 asm and the EnvironmentManager header attest.
    static void _AssertLayout()
    {
        static_assert(offsetof(GlobalIrradianceManager, maIrradianceMatrix)      == 0x000,
                      "maIrradianceMatrix must sit at +0x00 (GetIrradianceMatrix indexes colour<<6 from this)");
        static_assert(offsetof(GlobalIrradianceManager, mAverageIrradianceColour) == 0x0C0,
                      "mAverageIrradianceColour must sit at +0xC0 (ComputeIrradiance stvx128 r31+0xC0)");
        static_assert(offsetof(GlobalIrradianceManager, maFrameIrradianceCoeffs) == 0x0D0,
                      "maFrameIrradianceCoeffs must sit at +0xD0 (UpdateCoefficients +0xD0..+0x150)");
        static_assert(sizeof(GlobalIrradianceManager) == 0x160,
                      "GlobalIrradianceManager is 0x160 (EnvironmentManager +0x700 .. +0x860)");
    }
};
}

#endif // BRN_GLOBAL_IRRADIANCE_MANAGER_H
