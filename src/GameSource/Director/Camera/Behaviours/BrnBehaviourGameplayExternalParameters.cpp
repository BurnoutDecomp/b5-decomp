// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternalParameters.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayExternal::Parameters
// serialiser slice this TU owns (class:BrnDirector::Camera::BehaviourGameplayExternal::Parameters):
//   - BehaviourGameplayExternal::Parameters::Serialise<DebugMenuSerialiser>     @0x8224BBB0
//   - BehaviourGameplayExternal::Parameters::Serialise<TextFileWriteSerialiser> @0x8224D418
//   - BehaviourGameplayExternal::Parameters::Serialise<TextFileReadSerialiser>  @0x822312E8
//
// BehaviourGameplayExternal::Parameters itself (the external "chase" camera's authored tunings
// block, +0x00..+0xAC) is homed, fully laid out, in BrnBehaviourGameplayExternal.h; its seeding
// step Parameters::Set is bodied in BrnBehaviourGameplayExternal.cpp. This TU bodies only the ONE
// field-walk visitor declared there and instantiates it over the three camera-tunings serialisers
// (debug menu / text-file write / text-file read).
//
// UNLIKE the bumper/aftertouch serialisers (a single uniform ascending-offset walk), this block is
// VERSIONED: the visitor stamps the latest data version (3) into muVersion, serialises that version
// word, then dispatches on it. For the write/menu visitors the word is always 3 so the switch folds
// to case 3 (the DebugMenu instance @0x8224BBB0 emits case 3 straight-line, with no version word --
// its s32 Serialise overload is a no-op); for the read visitor the version word is read back from the
// file (fscanf "%s : %d"), so an old save can drive the case 1 / case 2 legacy branches, which read a
// different label set into a subset of the fields and default-fill the rest. The three cases below are
// transcribed store-for-store / call-for-call from the (fully-labelled) TextFileWriteSerialiser
// instance @0x8224D418; the read @0x822312E8 walks the identical case bodies, and the debug-menu
// @0x8224BBB0 the identical case 3 body.
//
// This mirrors the committed aftertouch-cam serialiser TU (BrnBehaviourAftertouchCamParameters.cpp)
// and bumper-cam serialiser TU (BrnBehaviourGameplayBumper.cpp): the visitor body plus one explicit
// instantiation per serialiser.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"  // Parameters (the block being walked)
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"         // DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"          // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"           // TextFileReadSerialiser

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BehaviourGameplayExternal::Parameters::Serialise<S> -- the ONE external-cam field-walk visitor body.
//
// Latest data version = 3. The two "Air Shake Params" / "Impact Shake Params" sub-sections at +0x0C
// and +0x1C are Utils::CameraShake::Parameters blocks (four f32 each) -- and since the RE-BASE
// (2026-07-29) they are declared as exactly that in BrnBehaviourGameplayExternal.h, so this visitor
// hands the serialiser the real members. It used to reach them through a reinterpret_cast over eight
// individually offset-named f32 slots (mfField0C..mfField28), because the block had no such members.
//
// Field/label map (from the fully-labelled write instance @0x8224D418):
//   version 3 (current): Air Shake + Impact Shake sub-blocks, then the seventeen f32 tunables below;
//   version 2 (legacy) : no shake sub-blocks, fourteen tunables, newer slots default-filled;
//   version 1 (legacy) : the original twenty-eight-field flat layout (different labels), newest
//                        three slots (+0x9C/+0xA0/+0xA4) default-filled.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourGameplayExternal::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    muVersion.muVersion = 3;                                         // stw r11(=3), 8(this) -- stamp the latest data version
    lrSerialiser.Serialise("Version Number (dont change)", muVersion.muVersion);

    switch (muVersion.muVersion)
    {
    case 1:
        // Legacy v1: the three slots introduced after v1 are default-filled, then the original
        // twenty-eight-field flat block is walked in ascending order.
        mfZAndTiltCutoffSpeedMPH = 100.0f;   // +0x9C
        mfSlideYScaleJump = 1.0f;     // +0xA0
        mfTiltAroundCarScale = 1.0f;     // +0xA4
        lrSerialiser.Serialise("Pitch Limit", mrPitchLimit);                     // +0x2C
        lrSerialiser.Serialise("Roll Limit", mrRollLimit);                      // +0x30
        lrSerialiser.Serialise("Pitch Coeff", mrPitchCoeff);                     // +0x34
        lrSerialiser.Serialise("Roll Coeff", mrRollCoeff);                      // +0x38
        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);                    // +0x3C
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);                      // +0x40
        lrSerialiser.Serialise("Acceleration Pitch", mrAccelerationPitchAmount);              // +0x44
        lrSerialiser.Serialise("Acceleration Sensitivity", mrAccelerationSensitivity);        // +0x48
        lrSerialiser.Serialise("Pivot Y", mrPivotY);                         // +0x4C
        lrSerialiser.Serialise("Pivot Z", mrPivotZ);                         // +0x50
        lrSerialiser.Serialise("Pivot Z Offset", mrPivotZOffset);                  // +0x54
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);                   // +0x58
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);                   // +0x5C
        lrSerialiser.Serialise("Slide Z Scale", mrSlideZScale);                   // +0x60
        lrSerialiser.Serialise("Slide Z Input for 50%slide", mrSlideZInputForHalf);      // +0x64
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);                     // +0x68
        lrSerialiser.Serialise("FOV", mrFOV);                                 // +0x6C
        lrSerialiser.Serialise("Look Front FOV Offset", mfInFrontFOVMax);           // +0x70
        lrSerialiser.Serialise("Look Front Towards Factor", mfFrontInAmount);       // +0x74
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);               // +0x78
        lrSerialiser.Serialise("Slide Z Speed Half limit", mfSpeedDisplacementHalf);        // +0x7C
        lrSerialiser.Serialise("Accel Z Lerp Amount", mfAccelZLerpAmount);             // +0x80
        lrSerialiser.Serialise("Z Lerp Amount", mfZLerpAmount);                   // +0x84
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);                // +0x88
        lrSerialiser.Serialise("Drift Yaw Spring", mfDriftYawSpring);                // +0x8C
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);          // +0x90
        lrSerialiser.Serialise("Down Angle", mfDownAngle);                      // +0x94
        lrSerialiser.Serialise("Velocity Slide Factor 0to1", mfVelocitySlideZFactor0To1);      // +0x98
        break;

    case 2:
        // Legacy v2: default-fill the fields v2 does not persist, then walk the v2 label set.
        mrPitchLimit = 8.0f;     mrRollLimit = 8.0f;     mrPitchCoeff = 0.75f;    mrRollCoeff = 0.0f;
        mfFrontInAmount = 0.0f;     mfVelocitySlideZFactor0To1 = 0.0f;     mrAccelerationPitchAmount = -0.5f;    mrAccelerationSensitivity = 0.015f;
        mrSlideZScale = 17.0f;    mrSlideZInputForHalf = 0.25f;    mfInFrontFOVMax = 60.0f;    mfSpeedDisplacementHalf = 0.01f;
        mfAccelZLerpAmount = 0.1f;     mfZLerpAmount = 0.7f;
        mfZAndTiltCutoffSpeedMPH = 100.0f;   mfSlideYScaleJump = 1.0f;     mfTiltAroundCarScale = 1.0f;
        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);                    // +0x3C
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);                      // +0x40
        lrSerialiser.Serialise("Yaw Spring in Drift", mfDriftYawSpring);             // +0x8C
        lrSerialiser.Serialise("Pivot Height", mrPivotY);                    // +0x4C
        lrSerialiser.Serialise("Pivot Length", mrPivotZ);                    // +0x50
        lrSerialiser.Serialise("Pivot Z Offset Along Car", mrPivotZOffset);        // +0x54
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);                   // +0x58
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);                   // +0x5C
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);                     // +0x68
        lrSerialiser.Serialise("FOV", mrFOV);                                 // +0x6C
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);               // +0x78
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);          // +0x90
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);                // +0x88
        lrSerialiser.Serialise("Down Angle", mfDownAngle);                      // +0x94
        break;

    case 3:
        // Current v3: default-fill the non-persisted tunables, then walk the two shake sub-blocks
        // followed by the seventeen persisted f32 tunables.
        mrPitchLimit = 8.0f;     mrRollLimit = 8.0f;     mrPitchCoeff = 0.75f;    mrRollCoeff = 0.0f;
        mfFrontInAmount = 0.0f;     mfVelocitySlideZFactor0To1 = 0.0f;     mrAccelerationPitchAmount = -0.5f;    mrAccelerationSensitivity = 0.015f;
        mrSlideZScale = 17.0f;    mrSlideZInputForHalf = 0.25f;    mfInFrontFOVMax = 60.0f;    mfSpeedDisplacementHalf = 0.01f;
        mfAccelZLerpAmount = 0.1f;     mfZLerpAmount = 0.7f;
        // ⭐ The two nested sub-sections are REAL MEMBERS now. They used to be driven through
        // a reinterpret_cast over four individually-named f32 slots each, because the block was
        // laid out by offset and had no CameraShake::Parameters member. No type-pun remains.
        lrSerialiser.Serialise("Air Shake Params", mAirShakeParams);        // +0x0C
        lrSerialiser.Serialise("Impact Shake Params", mImpactShakeParams);  // +0x1C
        lrSerialiser.Serialise("Pitch Spring", mrPitchSpring);                    // +0x3C
        lrSerialiser.Serialise("Yaw Spring", mrYawSpring);                      // +0x40
        lrSerialiser.Serialise("Yaw Spring in Drift", mfDriftYawSpring);             // +0x8C
        lrSerialiser.Serialise("Pivot Height", mrPivotY);                    // +0x4C
        lrSerialiser.Serialise("Pivot Length", mrPivotZ);                    // +0x50
        lrSerialiser.Serialise("Pivot Z Offset Along Car", mrPivotZOffset);        // +0x54
        lrSerialiser.Serialise("Slide X Scale", mrSlideXScale);                   // +0x58
        lrSerialiser.Serialise("Slide Y Scale", mrSlideYScale);                   // +0x5C
        lrSerialiser.Serialise("Slide Z Max", mrSlideZOutputMax);                     // +0x68
        lrSerialiser.Serialise("FOV", mrFOV);                                 // +0x6C
        lrSerialiser.Serialise("FOV during boost", mfBoostFOV);               // +0x78
        lrSerialiser.Serialise("FOV Anti-Zoom in Boost", mfBoostFOVZoomCompensation);          // +0x90
        lrSerialiser.Serialise("Z and Tilt Cutoff Speed MPH", mfZAndTiltCutoffSpeedMPH);     // +0x9C
        lrSerialiser.Serialise("Z Distance Scale", mfZDistanceScale);                // +0x88
        lrSerialiser.Serialise("Slide Y Scale Jump", mfSlideYScaleJump);              // +0xA0
        lrSerialiser.Serialise("Tilt Around Car Scale", mfTiltAroundCarScale);           // +0xA4
        lrSerialiser.Serialise("Down Angle", mfDownAngle);                      // +0x94
        break;

    default:
        // code/data version mismatch -- unconditional assert (BehaviourGameplayExternal.h:449).
        CGS_ASSERT(false,
                   "BehaviourGameplayExternal::Parameters : code/data version mismatch, have you got the latest data?");
        break;
    }
}

// Explicit instantiations -- one per serialiser this block is menu-mirrored / saved / loaded through.
template void BehaviourGameplayExternal::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourGameplayExternal::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGameplayExternal::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
