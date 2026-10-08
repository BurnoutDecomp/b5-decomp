// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternalParameters.cpp
//
// BehaviourGameplayExternal::Parameters::Serialise<TSerialiser> -- the external chase-camera
// tunings walk, one generic body instantiated over the three camera serialisers.
//
// The block is versioned (code version 3): the walk stamps 3 into the tag, hands the tag to the
// serialiser (the read can replace it with the file's version; the debug-menu leaf ignores it),
// then walks the field set of that version. Versions 1 and 2 are the legacy file layouts: each
// seeds the tunables its layout does not carry, then walks its own label set. Any other version
// asserts.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The current code version of the BehaviourGameplayExternal::Parameters block.
    const u32 KU_GAMEPLAY_EXTERNAL_PARAMETERS_VERSION = 3u;
} // namespace

template<class TSerialiser>
void BehaviourGameplayExternal::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    muVersion.muVersion = KU_GAMEPLAY_EXTERNAL_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    switch (muVersion.muVersion)
    {
    case 1:
        mfZAndTiltCutoffSpeedMPH = 100.0f;
        mfSlideYScaleJump        = 1.0f;
        mfTiltAroundCarScale     = 1.0f;

        lrSerialiser.Serialise("Pitch Limit", mrPitchLimit);
        lrSerialiser.Serialise("Roll Limit", mrRollLimit);
        lrSerialiser.Serialise("Pitch Coeff", mrPitchCoeff);
        lrSerialiser.Serialise("Roll Coeff", mrRollCoeff);
        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);
        lrSerialiser.Serialise("Acceleration Pitch", mrAccelerationPitchAmount);
        lrSerialiser.Serialise("Acceleration Sensitivity", mrAccelerationSensitivity);
        lrSerialiser.Serialise("Pivot Y", mrPivotY);
        lrSerialiser.Serialise("Pivot Z", mrPivotZ);
        lrSerialiser.Serialise("Pivot Z Offset", mrPivotZOffset);
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);
        lrSerialiser.Serialise("Slide Z Scale", mrSlideZScale);
        lrSerialiser.Serialise("Slide Z Input for 50%slide", mrSlideZInputForHalf);
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);
        lrSerialiser.Serialise("FOV", mrFOV);
        lrSerialiser.Serialise("Look Front FOV Offset", mfInFrontFOVMax);
        lrSerialiser.Serialise("Look Front Towards Factor", mfFrontInAmount);
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
        lrSerialiser.Serialise("Slide Z Speed Half limit", mfSpeedDisplacementHalf);
        lrSerialiser.Serialise("Accel Z Lerp Amount", mfAccelZLerpAmount);
        lrSerialiser.Serialise("Z Lerp Amount", mfZLerpAmount);
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);
        lrSerialiser.Serialise("Drift Yaw Spring", mfDriftYawSpring);
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);
        lrSerialiser.Serialise("Down Angle", mfDownAngle);
        lrSerialiser.Serialise("Velocity Slide Factor 0to1", mfVelocitySlideZFactor0To1);
        break;

    case 2:
        mrPitchLimit               = 8.0f;
        mrRollLimit                = 8.0f;
        mrPitchCoeff               = 0.75f;
        mrRollCoeff                = 0.0f;
        mfFrontInAmount            = 0.0f;
        mfVelocitySlideZFactor0To1 = 0.0f;
        mrAccelerationPitchAmount  = -0.5f;
        mrAccelerationSensitivity  = 0.015f;
        mrSlideZScale              = 17.0f;
        mrSlideZInputForHalf       = 0.25f;
        mfInFrontFOVMax            = 60.0f;
        mfSpeedDisplacementHalf    = 0.01f;
        mfAccelZLerpAmount         = 0.1f;
        mfZLerpAmount              = 0.7f;
        mfZAndTiltCutoffSpeedMPH   = 100.0f;
        mfSlideYScaleJump          = 1.0f;
        mfTiltAroundCarScale       = 1.0f;

        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);
        lrSerialiser.Serialise("Yaw Spring in Drift", mfDriftYawSpring);
        lrSerialiser.Serialise("Pivot Height", mrPivotY);
        lrSerialiser.Serialise("Pivot Length", mrPivotZ);
        lrSerialiser.Serialise("Pivot Z Offset Along Car", mrPivotZOffset);
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);
        lrSerialiser.Serialise("FOV", mrFOV);
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);
        lrSerialiser.Serialise("Down Angle", mfDownAngle);
        break;

    case 3:
        mrPitchLimit               = 8.0f;
        mrRollLimit                = 8.0f;
        mrPitchCoeff               = 0.75f;
        mrRollCoeff                = 0.0f;
        mfFrontInAmount            = 0.0f;
        mfVelocitySlideZFactor0To1 = 0.0f;
        mrAccelerationPitchAmount  = -0.5f;
        mrAccelerationSensitivity  = 0.015f;
        mrSlideZScale              = 17.0f;
        mrSlideZInputForHalf       = 0.25f;
        mfInFrontFOVMax            = 60.0f;
        mfSpeedDisplacementHalf    = 0.01f;
        mfAccelZLerpAmount         = 0.1f;
        mfZLerpAmount              = 0.7f;

        lrSerialiser.Serialise("Air Shake Params", mAirShakeParams);
        lrSerialiser.Serialise("Impact Shake Params", mImpactShakeParams);
        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);
        lrSerialiser.Serialise("Yaw Spring in Drift", mfDriftYawSpring);
        lrSerialiser.Serialise("Pivot Height", mrPivotY);
        lrSerialiser.Serialise("Pivot Length", mrPivotZ);
        lrSerialiser.Serialise("Pivot Z Offset Along Car", mrPivotZOffset);
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);
        lrSerialiser.Serialise("FOV", mrFOV);
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);
        lrSerialiser.Serialise("Z and Tilt Cutoff Speed MPH", mfZAndTiltCutoffSpeedMPH);
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);
        lrSerialiser.Serialise("Slide Y Scale Jump", mfSlideYScaleJump);
        lrSerialiser.Serialise("Tilt Around Car Scale", mfTiltAroundCarScale);
        lrSerialiser.Serialise("Down Angle", mfDownAngle);
        break;

    default:
        CGS_ASSERT(false,
                   "BehaviourGameplayExternal::Parameters : code/data version mismatch, have you got the latest data?");
        break;
    }
}

template void BehaviourGameplayExternal::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourGameplayExternal::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGameplayExternal::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
