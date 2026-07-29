#pragma once

#include "BrnCommonTypes.h"   // Vector3

// =============================================================================
// SharedClasses/World/BrnEnvironmentUtil.h
//
// The free sky/lighting maths in namespace BrnWorld::EnvironmentSettings. These are
// the functions the EnvironmentManager calls to turn a time of day into a sun
// direction and to evaluate the analytic sky model.
//
// X360 (BURNOUT_X360_ARTIST.XEX):
//     ComputeKeyLightDirection    @ 0x82678AB0   -- reconstructed
//     ComputeSkyColour            @ 0x8267C5C0   -- NOT reconstructed
//     ComputeIrradianceRigFromSky @ 0x8267C948   -- NOT reconstructed
//
// The latter two were previously recorded as unrecoverable because they are driven by
// an "undecoded .rdata vperm permute mask (unk_82CDA450)". That mask is no longer
// undecoded -- see the note in BrnEnvironmentUtil.cpp -- so they are now unblocked
// work, not blocked work. They are simply out of this wave's scope.
// =============================================================================

namespace BrnWorld
{
namespace EnvironmentSettings
{
    // @ 0x82678AB0.
    //
    // Turn a sun elevation angle into the world-space KEY LIGHT DIRECTION (the direction
    // the sunlight travels, i.e. from the sun toward the scene).
    //
    //   lfSunAngleRad       -- the sun's elevation, in radians, 0 at the eastern horizon
    //                          and pi at the western horizon (the caller maps seconds of
    //                          day through (t - 23400) * 2*pi/86400).
    //   lfRigRotationDeg    -- the whole sun rig's heading about Y, in degrees.
    //   lfTiltAtHorizonDeg  -- the rig's tilt about X at the horizon, in degrees.
    //   lfTiltAtMiddayDeg   -- the rig's tilt about X at midday, in degrees.
    //
    // The tilt is interpolated between the two by a triangular ramp that peaks at midday,
    // then the base direction (-cos, -sin, 0) is rotated by that tilt about X and by the
    // rig heading about Y.
    Vector3 ComputeKeyLightDirection( f32 lfSunAngleRad,
                                      f32 lfRigRotationDeg,
                                      f32 lfTiltAtHorizonDeg,
                                      f32 lfTiltAtMiddayDeg );
}
}
