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

    // ---- skyrig: ComputeIrradianceRigFromSky @0x8267C948 ----
    //
    // Re-derive the six FILL LIGHTS of the ambient irradiance rig from the analytic sky model
    // instead of taking the keyframe's authored ones. Six point samples of ComputeSkyColour -- one
    // per rig face -- written straight into the six LightingData colour slots
    // (BrnEnvLightingData.h mv3{Key,Shadow,Left,Right,Up,Down}FillColour), which
    // GlobalIrradianceManager then folds into the two irradiance quadrics. There is no sample loop,
    // no cosine weighting and no normalisation pass: six taps, six stores.
    //
    //   lKeyFillColour    <- the horizon in the sun's own azimuth      (Normalize(-K.x, 0, -K.z))
    //   lShadowFillColour <- the horizon directly opposite the sun     (Normalize( K.x, 0,  K.z))
    //   lRightFillColour  <- 90 degrees round from the sun             (Cross(keyFillDir, +Y))
    //   lLeftFillColour   <- that mirrored through the Y axis
    //   lUpFillColour     <- straight up   (0, +1, 0)
    //   lDownFillColour   <- straight down (0, -1, 0)
    //
    //   lTopColourDrk / lHorColourPow / lSunColourPow / lHorBleedSclPow -- the same four packed sky
    //   inputs ComputeSkyColour takes, forwarded unchanged to all six taps.
    //   lKeyLightDir -- the direction the key light TRAVELS (sun -> scene); each tap receives its
    //   negation as ComputeSkyColour's lSunDir.
    //
    // Parameter names are the original's (references/Feb-2007/BrnEntityModuleUnity/SharedClasses/
    // World/BrnEnvironmentUtil.h, which declares this exact eleven-parameter form); the shape is
    // DWARF-attested (references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentUtil.cpp,
    // source line 100). Feb-2007 spells the out-parameters `Vector3::OutParam`; the PC vendor
    // types.h carries no such typedef, so they are spelled as its expansion, Vector3&.
    //
    // [FLAG UNREACHABLE IN THE RETAIL FLOW] The sole caller,
    // EnvironmentManager::GenerateShaderConstants @0x827D0098, calls this only when
    // EnvironmentManager::mbSetIrradianceFromSky (+0x6F5) is true. Construct @0x827CA408 clears that
    // byte and NOTHING else in the shipped X360 image writes it; the only writer is the debug UI,
    // where DebugComponent::OnActivate @0x827B2408 registers it as the bool debug variable
    // "Set Irradiance rig from Sky" in the debug group "Irradiance rig". Reconstructed in full
    // anyway: it is real console code, and the sky model it samples is the one the sky dome draws.
    void ComputeIrradianceRigFromSky( Vector3& lKeyFillColour,
                                      Vector3& lShadowFillColour,
                                      Vector3& lLeftFillColour,
                                      Vector3& lRightFillColour,
                                      Vector3& lUpFillColour,
                                      Vector3& lDownFillColour,
                                      Vector4::InParam lTopColourDrk,
                                      Vector4::InParam lHorColourPow,
                                      Vector4::InParam lSunColourPow,
                                      Vector3::InParam lHorBleedSclPow,
                                      Vector3::InParam lKeyLightDir );

    // ---- skycolour: ComputeSkyColour @0x8267C5C0 ----
    //
    // The analytic sky model, evaluated on the CPU for ONE direction. It is the same model the sky
    // dome's own pixel shader runs per pixel: BrnSkyDomeManager::Render (BrnSkyDomeManager.cpp:647,
    // and RenderToEnvironmentMap :728) hands Im3dSkyDome::SetConstants exactly these five inputs, in
    // this order -- GetTopColourDrk() / GetHorColourPow() / GetSunColourPow() / GetHorBleedSclPow()
    // plus GetKeyLightDirection() -- so the CPU-side irradiance rig and the drawn dome agree by
    // construction. Parameter names are the original's (references/Feb-2007/BrnEntityModuleUnity/
    // SharedClasses/World/BrnEnvironmentUtil.h); the shape is DWARF-attested
    // (references/DecFIGS/dwarfdump/SharedClasses/World/BrnEnvironmentUtil.cpp, source line 50:
    // `Vector3 ComputeSkyColour(Vector4, Vector4, Vector4, Vector3, Vector3, Vector3)`).
    //
    // The .w packing of the three colour vectors is the keyframe's own ScatteringData layout
    // (GameSource/World/EnvironmentSettings/BrnEnvScatteringData.h):
    //
    //   lTopColourDrk   -- (mv3SkyTopColour.rgb, mfSkyDrk)      zenith colour + the away-from-sun
    //                                                            darkening (1 at the sun, 1-Drk opposite)
    //   lHorColourPow   -- (mv3SkyHorColour.rgb, mfSkyHorPow)   horizon colour + the horizon->zenith
    //                                                            ramp exponent
    //   lSunColourPow   -- (mv3SkySunColour.rgb, mfSkySunPow)   sun colour + the TIGHT sun-lobe exponent
    //   lHorBleedSclPow -- (mfSkyHorBleedScl, mfSkyHorBleedPow, mfSkySunBleedPow): how much / how
    //                      sharply the horizon colour bleeds up the dome toward the sun's AZIMUTH,
    //                      and the WIDE sun-lobe exponent
    //   lSunDir         -- unit direction TOWARD the sun. The only caller,
    //                      ComputeIrradianceRigFromSky @0x8267C948, passes the NEGATED key light
    //                      direction (the key light travels sun -> scene).
    //   lDir            -- the unit direction being sampled.
    //
    // Returns the sky colour at lDir. The w lane carries the incidental whole-register lerp the X360
    // leaves there (see BrnEnvironmentUtil.cpp); no consumer reads it.
    Vector3 ComputeSkyColour( Vector4::InParam lTopColourDrk,
                              Vector4::InParam lHorColourPow,
                              Vector4::InParam lSunColourPow,
                              Vector3::InParam lHorBleedSclPow,
                              Vector3::InParam lSunDir,
                              Vector3::InParam lDir );
}
}
