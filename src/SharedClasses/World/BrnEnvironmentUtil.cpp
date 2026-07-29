#include "SharedClasses/World/BrnEnvironmentUtil.h"

#include <cmath>   // sinf / cosf / fabsf

// =============================================================================
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnWorld::EnvironmentSettings::ComputeKeyLightDirection @ 0x82678AB0
//
// Called by EnvironmentManager::CalcKeyLightDirection @0x827B0638 and (twice) by
// EnvironmentManager::GenerateShaderConstants @0x827D0098 -- once with the raw time of
// day for the UNBIASED direction and once with the sun-elevation-clamped time for the
// biased one.
//
// NOTE ON THE TWO SIBLINGS IN THIS FILE'S DWARF HOME (ComputeSkyColour @0x8267C5C0 and
// ComputeIrradianceRigFromSky @0x8267C948): the ledger records them as blocked on an
// "UNDECODED .rdata vperm permute mask (unk_82CDA450) ... mask VALUES unrecoverable
// without fabrication". That is no longer true. The 16 bytes at 0x82CDA450 are
//     00 01 02 03 | 18 19 1A 1B | 00 01 02 03 | 00 01 02 03
// which, for a vperm whose two source operands are the same register, selects source
// words {0, 2, 0, 0} -- i.e. it is the ordinary (X, Z, X, X) lane gather used to take a
// direction's XZ-plane length. The same gather, on the same mask, is what
// BrnGraphics::Im3dSkyDome::SetConstants uses to build "KeyLightDirAndXZLength", and
// there the destination shader constant's own name confirms the reading independently.
// The two sky-scattering functions are therefore unblocked work, not blocked work; they
// are simply outside this wave's scope.
// =============================================================================

namespace
{
    // The X360 rodata this function loads, all now dumped from the ARTIST image:
    const f32 KF_DEG_TO_RAD  = 0.017453292f;   // flt_820A3674
    const f32 KF_HALF_PI     = 1.5707964f;     // flt_820A3684
    const f32 KF_TWO_OVER_PI = 0.63661975f;    // flt_820A82C4
    const f32 KF_ONE         = 1.0f;           // flt_82001C98
    const f32 KF_ZERO        = 0.0f;           // flt_82001CC0
}

namespace BrnWorld
{
namespace EnvironmentSettings
{

// @ 0x82678AB0
Vector3 ComputeKeyLightDirection( f32 lfSunAngleRad,
                                  f32 lfRigRotationDeg,
                                  f32 lfTiltAtHorizonDeg,
                                  f32 lfTiltAtMiddayDeg )
{
    // Tilt: a triangular ramp that is 0 at either horizon and 1 at midday
    // (lfSunAngleRad == pi/2), used to lerp horizon tilt -> midday tilt.
    const f32 lfTiltAtHorizonRad = lfTiltAtHorizonDeg * KF_DEG_TO_RAD;
    const f32 lfTiltDelta        = (lfTiltAtMiddayDeg * KF_DEG_TO_RAD) - lfTiltAtHorizonRad;
    const f32 lfMiddayFraction   =
        KF_ONE - std::fabs((lfSunAngleRad - KF_HALF_PI) * KF_TWO_OVER_PI);
    const f32 lfTiltRad          = (lfMiddayFraction * lfTiltDelta) + lfTiltAtHorizonRad;

    const f32 lfRigRotationRad   = lfRigRotationDeg * KF_DEG_TO_RAD;

    // The base direction, in the sun rig's own frame: the light travels TOWARD the scene,
    // so it is the negation of the direction to the sun.
    Vector3 lDirection;
    lDirection.x = -std::cos(lfSunAngleRad);
    lDirection.y = -std::sin(lfSunAngleRad);
    lDirection.z = KF_ZERO;
    lDirection.w = KF_ZERO;

    // Rotate about X by the tilt, then about Y by the rig heading. The X360 emits the two
    // row-vector * matrix products as vmulfp128 / vmaddfp ladders (0x82678BDC..0x82678C08);
    // they are lowered here to the equivalent lane math, per the project's vendor-SIMD rule.
    const f32 lfSinTilt = std::sin(lfTiltRad);
    const f32 lfCosTilt = std::cos(lfTiltRad);
    const f32 lfSinRig  = std::sin(lfRigRotationRad);
    const f32 lfCosRig  = std::cos(lfRigRotationRad);

    // v * RotationX(tilt)
    const f32 lfTiltedX = lDirection.x;
    const f32 lfTiltedY = (lDirection.y * lfCosTilt) + (lDirection.z * lfSinTilt);
    const f32 lfTiltedZ = (lDirection.z * lfCosTilt) - (lDirection.y * lfSinTilt);

    // (v * RotationX) * RotationY(rig)
    Vector3 lResult;
    lResult.x = (lfTiltedX * lfCosRig) - (lfTiltedZ * lfSinRig);
    lResult.y = lfTiltedY;
    lResult.z = (lfTiltedX * lfSinRig) + (lfTiltedZ * lfCosRig);
    lResult.w = KF_ZERO;
    return lResult;
}

}
}
