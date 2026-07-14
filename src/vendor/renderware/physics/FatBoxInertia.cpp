#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4
#include "types.hpp"             // f32

// ===========================================================================
// rw::physics::ComputeFatBoxInertia -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   rw::physics::ComputeFatBoxInertia  @ 0x82BC6C80   (BODIED below)
//
// Free function (no `this`): the X360 ABI passes the four half-extent/margin
// scalars in f1..f4 (they shadow GPR slots r3..r6, which the Hex-Rays pseudocode
// mis-modelled as unused int args a5..a8), and the destination register lane in
// r7 -- the pointer the closing `stvx128 v0, r0, r7` writes. The IDA PSEUDOCODE
// IS INCOMPLETE: it drops the three `stfs` stack stores that the final
// `lvx128/stvx128` copies out, and the whole diagonal-inertia / volume maths.
// This body is transcribed store-for-store from the RAW asm.
//
// Given a box with half-extents (x,y,z) grown by a rounding `margin` (its
// Minkowski sum with a sphere of radius `margin` -- the "fat box"), it fills the
// per-unit-mass diagonal inertia (Ixx,Iyy,Izz,0) into *out and RETURNS the fat
// box's volume in f1 (the value left live in the return register at `blr`).
//
// Recovered .rdata float constants (grounded, big-endian X360 DB):
//   flt_8200AECC 0.6666667f (2/3)   flt_82001D9C 2.0f    flt_82004C88 8.0f
//   flt_820065E0 0.33333334f (1/3)  flt_82001DA0 0.5f    flt_820047C8 0.05f
//   flt_820F5E68 1.0e-7f            flt_82001C94 6.2831855f (2*pi)
// ===========================================================================

namespace rw
{
namespace physics
{

f32 ComputeFatBoxInertia(f32 lfHalfX, f32 lfHalfY, f32 lfHalfZ, f32 lfMargin,
                         rw::math::vpu::Vector4* lpOut)
{
    // --- recovered .rdata float constants (used by value where the asm lfs's them) ---
    const f32 KF_TwoThirds = 0.6666667f;    // flt_8200AECC  -> f31
    const f32 KF_Two       = 2.0f;          // flt_82001D9C  -> f11
    const f32 KF_Eight     = 8.0f;          // flt_82004C88  -> f8
    const f32 KF_OneThird  = 0.33333334f;   // flt_820065E0  -> f10
    const f32 KF_Half      = 0.5f;          // flt_82001DA0  -> f13
    const f32 KF_FivePct   = 0.05f;         // flt_820047C8
    const f32 KF_Epsilon   = 1.0e-7f;       // flt_820F5E68
    const f32 KF_TwoPi     = 6.2831855f;    // flt_82001C94

    // fsel fD,fA,fB,fC  =>  fD = (fA >= 0.0f) ? fB : fC

    // 0.5 * margin^2, kept live for every inertia lane (f12*f13 -> f9 @ 0x82BC6CCC).
    const f32 lfHalfMarginSq = (lfMargin * lfMargin) * KF_Half;

    // Largest half-extent: max(x, max(y, z)).
    const f32 lfMaxYZ  = ((lfHalfY - lfHalfZ) >= 0.0f) ? lfHalfY : lfHalfZ;   // 0x82BC6CB0
    const f32 lfMaxXYZ = ((lfHalfX - lfMaxYZ) >= 0.0f) ? lfHalfX : lfMaxYZ;   // 0x82BC6CD4

    // Minimum skin thickness: max((maxExtent + margin) * 0.05, 1e-7).
    const f32 lfScaled = (lfMaxXYZ + lfMargin) * KF_FivePct;                          // 0x82BC6CD8/CE4
    const f32 lfSkin   = ((lfScaled - KF_Epsilon) >= 0.0f) ? lfScaled : KF_Epsilon;   // 0x82BC6CF0/CF4

    // Clamp each half-extent up to (skin - margin) so a thin box never collapses.
    const f32 lfFloor = lfSkin - lfMargin;                                    // 0x82BC6CF8
    const f32 lfY = ((lfHalfY - lfFloor) >= 0.0f) ? lfHalfY : lfFloor;        // 0x82BC6D08 (f0)
    const f32 lfZ = ((lfHalfZ - lfFloor) >= 0.0f) ? lfHalfZ : lfFloor;        // 0x82BC6D0C (f13)
    const f32 lfX = ((lfHalfX - lfFloor) >= 0.0f) ? lfHalfX : lfFloor;        // 0x82BC6D10 (f12)

    // --- per-axis squares (f7=y^2 @ D14, f5=z^2 @ D30, f3=x^2 @ D34) ---
    const f32 lfXX = lfX * lfX;
    const f32 lfYY = lfY * lfY;
    const f32 lfZZ = lfZ * lfZ;

    // --- diagonal inertia (per unit mass): I_axis = (1/3)*(sum of the two other
    //     squares + 2*margin*(sum of the two other extents)) + 0.5*margin^2 ---

    // Ixx depends on (y,z)  -> lane 0  (f31=(y+z)*margin @ D68; f11 @ D74/D88; f12 @ D94)
    const f32 lfIxx = (((lfY + lfZ) * lfMargin) * KF_Two + lfZZ + lfYY) * KF_OneThird + lfHalfMarginSq;
    // Iyy depends on (x,z)  -> lane 1  (f2=(x+z)*margin @ D3C; f2 @ D50; f6 @ D6C; f12 @ D7C)
    const f32 lfIyy = (((lfX + lfZ) * lfMargin) * KF_Two + lfXX + lfZZ) * KF_OneThird + lfHalfMarginSq;
    // Izz depends on (x,y)  -> lane 2  (f1=(x+y)*margin @ D40; f3 @ D54; f5 @ D78; f12 @ D8C)
    const f32 lfIzz = (((lfX + lfY) * lfMargin) * KF_Two + lfXX + lfYY) * KF_OneThird + lfHalfMarginSq;

    // Store the 16-byte lane register (var_30..var_24) that the closing
    // lvx128/stvx128 copies straight to *out. (stfs var_30/2C/28 + stw 0 @ var_24.)
    lpOut->x = lfIxx;   // var_30 -> lane 0   (0x82BC6D98)
    lpOut->y = lfIyy;   // var_2C -> lane 1   (0x82BC6D80)
    lpOut->z = lfIzz;   // var_28 -> lane 2   (0x82BC6D90)
    lpOut->w = 0.0f;    // var_24 -> lane 3   (stw r9, r9 = 0 @ 0x82BC6D9C)

    // --- return value: fat-box (Minkowski-sum) volume, left live in f1 @ blr ---
    // box core                8*x*y*z                                 (f31=xyz @ D44; f13=8xyz @ D58)
    const f32 lfBoxVolume = KF_Eight * ((lfX * lfY) * lfZ);
    // face slabs + rounded edges/corners:
    //   8*(xy+xz+yz)                                                  (f12 @ D48; *8 @ D70)
    const f32 lfFaceArea  = KF_Eight * (((lfY + lfZ) * lfX) + (lfY * lfZ));
    //   2*pi * margin * (x+y+z + (2/3)*margin)                        (f2 @ D1C; f0 @ D38/D4C; f31 @ D5C; f0 @ D64)
    const f32 lfRound     = ((((lfMargin * KF_TwoThirds) + lfX) + lfY + lfZ) * lfMargin) * KF_TwoPi;
    // surface term (f0 @ D70) then * margin + box core (f1 @ D84).
    const f32 lfSurface   = lfFaceArea + lfRound;
    return lfSurface * lfMargin + lfBoxVolume;
}

} // namespace physics
} // namespace rw
