// ============================================================================
// GameSource/Director/Camera/BrnBehaviourParameterBank.cpp
//
// BrnDirector::Camera::BehaviourParameterBank -- Construct (with the record's own seed,
// NamedParameters::Construct), the bank's serialiser walks, SaveParameters / LoadParameters, the
// SerialiseBehaviourParameters type-tag dispatch and the two bank-only serialisers' visitors.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"                  // the five serialisers + SerialiseBehaviourParameters
#include "GameSource/Director/Arbitrator/States/BrnArbStateTestbed.h"             // ArbStateTestbed::GenericActivateCam (TestbedSetupSerialiser)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugCompononent.h" // DebugComponent::RegisterFunction (TestbedSetupSerialiser)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                // Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                     // CgsDev::StrStream (the unknown-tag message)

namespace BrnDirector
{

// ----------------------------------------------------------------------------
// NamedParameters::Construct -- the record half of the bank's Construct, in the console's order:
// each block's own Parameters::Construct (or the console's inlined copy of it), then the bank's
// authored re-tunes and the block copies they are built from.
// ----------------------------------------------------------------------------
void NamedParameters::Construct()
{
    mAftertouchCamDefault.Construct();
    mAftertouchCrashParams.Construct();
    mCrashDebugParams.Construct();
    mHeliCamDefaultParams.Construct();

    mGyroCamDefaultParams.Construct();
    mGyroCamTruckFront.Construct();
    mGyroCamLeft.Construct();
    mGyroCamRight.Construct();
    mGyroCamDefaultSideTruckingLeftParams.Construct();
    mGyroCamDefaultSideTruckingRightParams.Construct();
    mGyroCamFollow.Construct();
    mGyroCamAlwaysLowParams.Construct();
    mGyroCamTakedownParams.Construct();
    mGyroCamTakedownZoomedOutParams.Construct();
    mGyroCamHighParams.Construct();
    mGyroCamHelicamParams.Construct();
    mGyroCamDriveByLParams.Construct();
    mGyroCamDriveByRParams.Construct();

    mBystanderJumpLeftParameters.Construct();
    mBystanderJumpParameters2.Construct();
    mBystanderJumpFromBehindParameters.Construct();
    mBystanderCloseParameters.Construct();
    mBystanderMediumParameters.Construct();
    mBystanderFarParameters.Construct();
    mBystanderFarTallParameters.Construct();

    mRigBonnetLowRight.Construct();
    mRigRearQFwd.Construct();
    mRigFrontQCuFwd.Construct();
    mRigFrontQBwd.Construct();
    mRigFrontRearview.Construct();
    mRigBootViewFwd.Construct();
    mRigUnderbelly.Construct();
    mRigFrontQLowBwd.Construct();
    mRigRoofFwd.Construct();
    mRigBootFwd.Construct();
    mRigFrontQCuFwd2.Construct();
    mRigDropUnderbelly.Construct();
    mRigDropFrontQCuFwd.Construct();
    mRigDropBootViewFwd.Construct();

    mFailsafe.Construct();

    mPassengerDefault.Construct();

    mLooseAttachmentTakedown1.Construct();
    mLooseAttachmentTakedown2.Construct();
    mLooseAttachmentTakedown3.Construct();

    mFixedDefault.Construct();
    maLookAroundCarCamParameters.Construct();
    maSpirallingDeathcamParameters.Construct();
    mRoadRunnerDefault.Construct();

    // ---- the gyro run ----
    mGyroCamDefaultParams.mLookerParams.mfTrackingTolerance            = 0.1f;
    mGyroCamDefaultParams.mShakeParams.mfWobbleCenteringFactor         = 1.0f;
    mGyroCamDefaultParams.mLookerParams.mbInitialiseToLookingAtTarget  = true;
    mGyroCamDefaultParams.mLookerParams.mbUseZoom                      = false;
    mGyroCamDefaultParams.mfSlowDistance                               = 3.5f;
    mGyroCamDefaultParams.mfSlowPitch                                  = -4.0f;

    mGyroCamDefaultSideTruckingLeftParams                 = mGyroCamDefaultParams;
    mGyroCamDefaultSideTruckingLeftParams.mbUseTruck      = true;
    mGyroCamDefaultSideTruckingLeftParams.mbUseSideVector = true;
    mGyroCamDefaultSideTruckingRightParams                = mGyroCamDefaultSideTruckingLeftParams;
    mGyroCamDefaultSideTruckingRightParams.mbInvertVector = true;

    mGyroCamFollow                = mGyroCamDefaultParams;
    mGyroCamFollow.mbInvertVector = true;

    mGyroCamAlwaysLowParams                = mGyroCamDefaultParams;
    mGyroCamAlwaysLowParams.mfFastPitch    = -4.0f;
    mGyroCamAlwaysLowParams.mfSlowPitch    = -4.0f;
    mGyroCamAlwaysLowParams.mfFastDistance = 9.0f;
    mGyroCamAlwaysLowParams.mfSlowDistance = 9.0f;
    mGyroCamAlwaysLowParams.mfFastHeight   = 0.2f;
    mGyroCamAlwaysLowParams.mfSlowHeight   = 0.2f;

    mGyroCamTakedownParams.mLookerParams.mfTrackingTolerance           = 0.1f;
    mGyroCamTakedownParams.mShakeParams.mfWobbleCenteringFactor        = 1.0f;
    mGyroCamTakedownParams.mLookerParams.mbInitialiseToLookingAtTarget = true;
    mGyroCamTakedownParams.mShakeParams.mfXYShakeMagnitudeDegs         = 0.15f;
    mGyroCamTakedownParams.mLookerParams.mbUseZoom                     = false;
    mGyroCamTakedownParams.mShakeParams.mfZShakeMagnitudeDegs          = 0.05f;
    mGyroCamTakedownParams.mbStickToGround                             = false;
    mGyroCamTakedownParams.mShakeParams.mfXYWobbleMagnitudeDegs        = 4.0f;
    mGyroCamTakedownParams.mfSlowDistance                              = 4.0f;
    mGyroCamTakedownParams.mfFastDistance                              = 8.0f;
    mGyroCamTakedownParams.mfSlowHeight                                = 1.0f;
    mGyroCamTakedownParams.mfSlowPitch                                 = -1.0f;
    mGyroCamTakedownParams.mfFastHeight                                = 1.5f;

    mGyroCamTakedownZoomedOutParams                = mGyroCamTakedownParams;
    mGyroCamTakedownZoomedOutParams.mfSlowDistance = 8.0f;
    mGyroCamTakedownZoomedOutParams.mfFastDistance = 8.0f;

    mGyroCamHighParams.mShakeParams.mfWobbleCenteringFactor        = 1.0f;
    mGyroCamHighParams.mLookerParams.mfTrackingTolerance           = 0.1f;
    mGyroCamHighParams.mfSlowHeight                                = 5.0f;
    mGyroCamHighParams.mLookerParams.mbInitialiseToLookingAtTarget = true;
    mGyroCamHighParams.mfSlowDistance                              = 9.0f;
    mGyroCamHighParams.mLookerParams.mbUseZoom                     = false;
    mGyroCamHighParams.mfFastHeight                                = 5.0f;
    mGyroCamHighParams.mbStickToGround                             = false;
    mGyroCamHighParams.mfFastDistance                              = 18.0f;

    mGyroCamHelicamParams.mLookerParams.mbInitialiseToLookingAtTarget = true;
    mGyroCamHelicamParams.mLookerParams.mfTrackingTolerance           = 0.1f;
    mGyroCamHelicamParams.mLookerParams.mbUseZoom                     = false;
    mGyroCamHelicamParams.mbStickToGround                             = false;
    mGyroCamHelicamParams.mfSlowHeight                                = 20.0f;
    mGyroCamHelicamParams.mfFastHeight                                = 20.0f;

    mGyroCamDriveByLParams                                             = mGyroCamDefaultParams;
    mGyroCamDriveByLParams.mAttachmentTruckParams.mfInitialOffsetDist   = -4.0f;
    mGyroCamDriveByLParams.mbUseTruck                                   = true;
    mGyroCamDriveByLParams.mbUseSideVector                              = true;
    mGyroCamDriveByLParams.mAttachmentTruckParams.mfConvergenceTimeSecs = 0.125f;
    mGyroCamDriveByRParams                                              = mGyroCamDriveByLParams;
    mGyroCamDriveByRParams.mbInvertVector                               = true;

    // ---- the bystander run ----
    mBystanderJumpLeftParameters.mLookerParams.mfDesiredPerceivedDistance = 5.0f;
    mBystanderJumpLeftParameters.mfTargetSpaceZ                           = 4.0f;
    mBystanderJumpLeftParameters.mLookerParams.mfTrackingTolerance        = 0.2f;
    mBystanderJumpLeftParameters.mLookerParams.mbUseZoom                  = false;
    mBystanderJumpLeftParameters.mLookerParams.mfTrackingSpeed            = 0.1f;
    mBystanderJumpLeftParameters.mbUseTargetSpaceInsteadOfPositionFinder  = true;
    mBystanderJumpLeftParameters.mfDistanceForFailKM                      = 0.0125f;
    mBystanderJumpLeftParameters.mfTargetSpaceY                           = -2.0f;
    mBystanderJumpLeftParameters.mbUseRangeTesting                        = false;
    mBystanderJumpLeftParameters.mfTargetSpaceX                           = 2.0f;

    mBystanderJumpParameters2.mLookerParams.mfDesiredPerceivedDistance = 5.0f;
    mBystanderJumpParameters2.mfDistanceForFailKM                      = 0.0125f;
    mBystanderJumpParameters2.mfTargetSpaceX                           = 0.0f;
    mBystanderJumpParameters2.mfTargetSpaceZ                           = 4.0f;
    mBystanderJumpParameters2.mfTargetSpaceY                           = -2.0f;
    mBystanderJumpParameters2.mLookerParams.mfTrackingTolerance        = 0.2f;
    mBystanderJumpParameters2.mLookerParams.mfTrackingSpeed            = 0.1f;
    mBystanderJumpParameters2.mLookerParams.mbUseZoom                  = false;
    mBystanderJumpParameters2.mbUseTargetSpaceInsteadOfPositionFinder  = true;
    mBystanderJumpParameters2.mbUseRangeTesting                        = false;

    mBystanderJumpFromBehindParameters.mfDistanceForFailKM                      = 0.0125f;
    mBystanderJumpFromBehindParameters.mLookerParams.mfTrackingTolerance        = 0.2f;
    mBystanderJumpFromBehindParameters.mLookerParams.mfDesiredPerceivedDistance = 5.0f;
    mBystanderJumpFromBehindParameters.mfTargetSpaceX                           = 1.1f;
    mBystanderJumpFromBehindParameters.mfTargetSpaceY                           = -0.78f;
    mBystanderJumpFromBehindParameters.mfTargetSpaceZ                           = -3.31f;
    mBystanderJumpFromBehindParameters.mLookerParams.mbUseZoom                  = false;
    mBystanderJumpFromBehindParameters.mLookerParams.mfTrackingSpeed            = 0.1f;
    mBystanderJumpFromBehindParameters.mbUseTargetSpaceInsteadOfPositionFinder  = true;
    mBystanderJumpFromBehindParameters.mbUseRangeTesting                        = false;

    mBystanderMediumParameters.mLookerParams.mfDesiredPerceivedDistance = 5.0f;
    mBystanderMediumParameters.mfMaxInitialDistanceKM                   = 0.03f;
    mBystanderMediumParameters.mfVelocityInfluenceOnPosition            = 0.5f;
    mBystanderMediumParameters.mfDistanceForFailKM                      = 0.06f;

    mBystanderFarParameters.mLookerParams.mfDesiredPerceivedDistance       = 8.0f;
    mBystanderFarParameters.mLookerParams.mfTrackingTolerance              = 0.5f;
    mBystanderFarParameters.mLookerParams.mfMinFOVVelocity                 = 120.0f;
    mBystanderFarParameters.mLookerParams.mfMaxFOVVelocity                 = 130.0f;
    mBystanderFarParameters.mLookerParams.mfToleranceForDistanceFromIdeal  = 20.0f;
    mBystanderFarParameters.mLookerParams.mfToleranceForDistanceFromTarget = 0.1f;
    mBystanderFarParameters.mfMaxInitialDistanceKM                         = 0.04f;
    mBystanderFarParameters.mfDistanceForFailKM                            = 0.06f;
    mBystanderFarParameters.mfVelocityInfluenceOnPosition                  = 0.75f;

    mBystanderCloseParameters                                         = mBystanderFarParameters;
    mBystanderCloseParameters.mLookerParams.mfDesiredPerceivedDistance = 4.0f;

    mBystanderFarTallParameters.mLookerParams.mfDesiredPerceivedDistance = 4.0f;
    mBystanderFarTallParameters.mfMaxInitialDistanceKM                   = 0.06f;
    mBystanderFarTallParameters.mfDistanceForFailKM                      = 1.2f;
    mBystanderFarTallParameters.mfHeight                                 = 2.5f;
    mBystanderFarTallParameters.mfVelocityInfluenceOnPosition            = 0.75f;

    // ---- the rig run: every rig after the first two is a copy of RearQFwd (the three "Drop" rigs
    // of DropUnderbelly) with its own authored rig preset ----
    mRigBonnetLowRight.mbUseAccelSpring = false;
    mRigBonnetLowRight.mRigParams       = Camera::Utils::CameraRig::ParamsBonnetLow;
    mRigBonnetLowRight.mbUseShake       = false;

    mRigRearQFwd.mbUseAccelSpring                    = false;
    mRigRearQFwd.mRigParams                          = Camera::Utils::CameraRig::ParamsRearQFwd;
    mRigRearQFwd.mPositionLagParams.mfXResponse      = 0.5f;
    mRigRearQFwd.mPositionLagParams.mfYResponse      = 0.5f;
    mRigRearQFwd.mPositionLagParams.mfZResponse      = 0.5f;
    mRigRearQFwd.mOrientationLagParams.mfSlerpSpring = 0.15f;
    mRigRearQFwd.mbUseOrientationLag                 = true;
    mRigRearQFwd.mbUsePositionLag                    = true;

    mRigFrontQCuFwd            = mRigRearQFwd;
    mRigFrontQCuFwd.mRigParams = Camera::Utils::CameraRig::ParamsFrontQCuFwd;
    mRigFrontQBwd              = mRigRearQFwd;
    mRigFrontQBwd.mRigParams   = Camera::Utils::CameraRig::ParamsRigFrontQBwd;
    mRigFrontRearview          = mRigRearQFwd;
    mRigFrontRearview.mRigParams = Camera::Utils::CameraRig::ParamsFrontRearview;
    mRigBootViewFwd            = mRigRearQFwd;
    mRigBootViewFwd.mRigParams = Camera::Utils::CameraRig::ParamsBootViewFwd;
    mRigFrontQLowBwd           = mRigRearQFwd;
    mRigFrontQLowBwd.mbUseAccelSpring = false;
    mRigRoofFwd                = mRigRearQFwd;
    mRigRoofFwd.mRigParams     = Camera::Utils::CameraRig::ParamsRoofFwd;
    mRigBootFwd                = mRigRearQFwd;
    mRigBootFwd.mbUseAccelSpring = false;
    mRigFrontQCuFwd2           = mRigRearQFwd;
    mRigFrontQCuFwd2.mRigParams = Camera::Utils::CameraRig::ParamsFrontQCuFwd2;
    mRigUnderbelly             = mRigRearQFwd;
    mRigUnderbelly.mRigParams  = Camera::Utils::CameraRig::ParamsUnderbelly;

    mRigDropUnderbelly                                   = mRigRearQFwd;
    mRigDropUnderbelly.mRigParams                        = Camera::Utils::CameraRig::ParamsUnderbelly;
    mRigDropUnderbelly.mShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;
    mRigDropUnderbelly.mShakeParams.mfWobbleCenteringFactor = 0.11f;
    mRigDropUnderbelly.mShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;
    mRigDropFrontQCuFwd            = mRigDropUnderbelly;
    mRigDropFrontQCuFwd.mRigParams = Camera::Utils::CameraRig::ParamsFrontQCuFwd;
    mRigDropBootViewFwd            = mRigDropUnderbelly;
    mRigDropBootViewFwd.mRigParams = Camera::Utils::CameraRig::ParamsBootViewFwd;

    // ---- the three shutdown-takedown loose-attachment beats ----
    Camera::BehaviourLooseAttachment::Parameters* const lapBeats[] = {
        &mLooseAttachmentTakedown1, &mLooseAttachmentTakedown2, &mLooseAttachmentTakedown3};
    for (u32 luBeat = 0; luBeat < 3; ++luBeat)
    {
        Camera::BehaviourLooseAttachment::Parameters& lrBeat = *lapBeats[luBeat];
        lrBeat.mImpact.mShakeParams.mfXYShakeMagnitudeDegs  = 0.1f;
        lrBeat.mImpact.mShakeParams.mfXYWobbleMagnitudeDegs = 0.0f;
        lrBeat.mImpact.mfShakeDecayFactor                   = 0.15f;
        lrBeat.mImpact.mfShakeMagnitude                     = 45.0f;
        lrBeat.mImpact.mfShakeFrequencyScale                = 2.5f;
        lrBeat.mfDutch                                      = 10.0f * static_cast<f32>(luBeat + 1);
        lrBeat.mfField54                                    = luBeat == 0 ? 90.0f : (luBeat == 1 ? 60.0f : 40.0f);
        lrBeat.mfDistance                                   = luBeat == 0 ? 6.0f : 5.0f;
        lrBeat.mfHeight                                     = 0.25f;
        lrBeat.mbLookFromTarget                             = true;
    }

    mGyroCamTruckFront                                       = mGyroCamDefaultParams;
    mGyroCamTruckFront.mAttachmentTruckParams.mfInitialOffsetDist   = 7.5f;
    mGyroCamTruckFront.mAttachmentTruckParams.mfConvergenceTimeSecs = 2.0f;
    mGyroCamTruckFront.mbUseTruck                            = true;
    mGyroCamLeft                 = mGyroCamDefaultParams;
    mGyroCamLeft.mbUseSideVector = true;
    mGyroCamRight                 = mGyroCamDefaultParams;
    mGyroCamRight.mbUseSideVector = true;
    mGyroCamRight.mbInvertVector  = true;

    // ---- the two aftertouch-crash blocks ----
    mAftertouchCrashParams.mfFastHeight = 0.5f;
    mAftertouchCrashParams.mfSlowHeight = 0.5f;
    mCrashDebugParams.mfSlowDistance    = 3.75f;
    mCrashDebugParams.mfFastDistance    = 15.0f;
    mCrashDebugParams.mfSlowHeight      = 1.875f;
    mCrashDebugParams.mfFastHeight      = 7.5f;
    mCrashDebugParams.mfFOV             = 50.0f;
    mCrashDebugParams.mfPitch           = 25.0f;

    // ---- the look-around (rotate-about-vehicle) block: target-subject size and screen offset ----
    maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXScreenOffset =  0.125f;
    maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectXSize         =  0.75f;
    maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYScreenOffset = -0.125f;
    maLookAroundCarCamParameters.mLookerParams.mfTargetSubjectYSize         =  0.75f;
}

namespace Camera
{

// ----------------------------------------------------------------------------
// BehaviourParameterBank::Serialise<T> -- stamp the current file version, hand the version word
// to the serialiser (a read replaces it with the file's), then walk the block list that version
// carries. A serialiser whose version leaf is a no-op (testbed, naming) always walks version 5.
// ----------------------------------------------------------------------------
template<class T>
void BehaviourParameterBank::Serialise(T& lrSerialiser)
{
    const u32 luCurrentVersion = 5;
    muVersion.muVersion = luCurrentVersion;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    switch (muVersion.muVersion)
    {
    case 1:
        SerialiseBehaviourParameters("Aftertouch", mNamedParameters.mAftertouchCamDefault, lrSerialiser);
        SerialiseBehaviourParameters("Aftertouch Crash", mNamedParameters.mAftertouchCrashParams, lrSerialiser);
        SerialiseBehaviourParameters("HeliCam Default", mNamedParameters.mHeliCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default", mNamedParameters.mGyroCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Always Low", mNamedParameters.mGyroCamAlwaysLowParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown", mNamedParameters.mGyroCamTakedownParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam High", mNamedParameters.mGyroCamHighParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Helicam", mNamedParameters.mGyroCamHelicamParams, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump", mNamedParameters.mBystanderJumpLeftParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Close", mNamedParameters.mBystanderCloseParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Medium", mNamedParameters.mBystanderMediumParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far", mNamedParameters.mBystanderFarParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far Tall", mNamedParameters.mBystanderFarTallParameters, lrSerialiser);
        SerialiseBehaviourParameters("Rig Bonnet Low Right", mNamedParameters.mRigBonnetLowRight, lrSerialiser);
        SerialiseBehaviourParameters("Rig Rear Q Fwd", mNamedParameters.mRigRearQFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd", mNamedParameters.mRigFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Bwd", mNamedParameters.mRigFrontQBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Rearview", mNamedParameters.mRigFrontRearview, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot View Fwd", mNamedParameters.mRigBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Low Bwd", mNamedParameters.mRigFrontQLowBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Roof Fwd", mNamedParameters.mRigRoofFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot Fwd", mNamedParameters.mRigBootFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd 2", mNamedParameters.mRigFrontQCuFwd2, lrSerialiser);
        SerialiseBehaviourParameters("Rig Underbelly", mNamedParameters.mRigUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Failsafe", mNamedParameters.mFailsafe, lrSerialiser);
        SerialiseBehaviourParameters("Passenger", mNamedParameters.mPassengerDefault, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown1", mNamedParameters.mLooseAttachmentTakedown1, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown2", mNamedParameters.mLooseAttachmentTakedown2, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown3", mNamedParameters.mLooseAttachmentTakedown3, lrSerialiser);
        break;
    case 2:
        SerialiseBehaviourParameters("Aftertouch", mNamedParameters.mAftertouchCamDefault, lrSerialiser);
        SerialiseBehaviourParameters("Aftertouch Crash", mNamedParameters.mAftertouchCrashParams, lrSerialiser);
        SerialiseBehaviourParameters("HeliCam Default", mNamedParameters.mHeliCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default", mNamedParameters.mGyroCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default Truck Left", mNamedParameters.mGyroCamDefaultSideTruckingLeftParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default Truck Right", mNamedParameters.mGyroCamDefaultSideTruckingRightParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Follow", mNamedParameters.mGyroCamFollow, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Always Low", mNamedParameters.mGyroCamAlwaysLowParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown", mNamedParameters.mGyroCamTakedownParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam High", mNamedParameters.mGyroCamHighParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Helicam", mNamedParameters.mGyroCamHelicamParams, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump Left", mNamedParameters.mBystanderJumpLeftParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump2", mNamedParameters.mBystanderJumpParameters2, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump From Behind", mNamedParameters.mBystanderJumpFromBehindParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Close", mNamedParameters.mBystanderCloseParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Medium", mNamedParameters.mBystanderMediumParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far", mNamedParameters.mBystanderFarParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far Tall", mNamedParameters.mBystanderFarTallParameters, lrSerialiser);
        SerialiseBehaviourParameters("Rig Bonnet Low Right", mNamedParameters.mRigBonnetLowRight, lrSerialiser);
        SerialiseBehaviourParameters("Rig Rear Q Fwd", mNamedParameters.mRigRearQFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd", mNamedParameters.mRigFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Bwd", mNamedParameters.mRigFrontQBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Rearview", mNamedParameters.mRigFrontRearview, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot View Fwd", mNamedParameters.mRigBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Low Bwd", mNamedParameters.mRigFrontQLowBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Roof Fwd", mNamedParameters.mRigRoofFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot Fwd", mNamedParameters.mRigBootFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd 2", mNamedParameters.mRigFrontQCuFwd2, lrSerialiser);
        SerialiseBehaviourParameters("Rig Underbelly", mNamedParameters.mRigUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Underbelly", mNamedParameters.mRigDropUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Front Q Cu Fwd", mNamedParameters.mRigDropFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Boot Q Cu Fwd", mNamedParameters.mRigDropBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Failsafe", mNamedParameters.mFailsafe, lrSerialiser);
        SerialiseBehaviourParameters("Passenger", mNamedParameters.mPassengerDefault, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown1", mNamedParameters.mLooseAttachmentTakedown1, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown2", mNamedParameters.mLooseAttachmentTakedown2, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown3", mNamedParameters.mLooseAttachmentTakedown3, lrSerialiser);
        SerialiseBehaviourParameters("Fixed Cam Default", mNamedParameters.mFixedDefault, lrSerialiser);
        break;
    case 3:
        SerialiseBehaviourParameters("Aftertouch", mNamedParameters.mAftertouchCamDefault, lrSerialiser);
        SerialiseBehaviourParameters("Aftertouch Crash", mNamedParameters.mAftertouchCrashParams, lrSerialiser);
        SerialiseBehaviourParameters("HeliCam Default", mNamedParameters.mHeliCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default", mNamedParameters.mGyroCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Front", mNamedParameters.mGyroCamTruckFront, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Left", mNamedParameters.mGyroCamLeft, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Right", mNamedParameters.mGyroCamRight, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Left", mNamedParameters.mGyroCamDefaultSideTruckingLeftParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Right", mNamedParameters.mGyroCamDefaultSideTruckingRightParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Follow", mNamedParameters.mGyroCamFollow, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Always Low", mNamedParameters.mGyroCamAlwaysLowParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown", mNamedParameters.mGyroCamTakedownParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam High", mNamedParameters.mGyroCamHighParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Helicam", mNamedParameters.mGyroCamHelicamParams, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump Left", mNamedParameters.mBystanderJumpLeftParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump2", mNamedParameters.mBystanderJumpParameters2, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump From Behind", mNamedParameters.mBystanderJumpFromBehindParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Close", mNamedParameters.mBystanderCloseParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Medium", mNamedParameters.mBystanderMediumParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far", mNamedParameters.mBystanderFarParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far Tall", mNamedParameters.mBystanderFarTallParameters, lrSerialiser);
        SerialiseBehaviourParameters("Rig Bonnet Low Right", mNamedParameters.mRigBonnetLowRight, lrSerialiser);
        SerialiseBehaviourParameters("Rig Rear Q Fwd", mNamedParameters.mRigRearQFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd", mNamedParameters.mRigFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Bwd", mNamedParameters.mRigFrontQBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Rearview", mNamedParameters.mRigFrontRearview, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot View Fwd", mNamedParameters.mRigBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Low Bwd", mNamedParameters.mRigFrontQLowBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Roof Fwd", mNamedParameters.mRigRoofFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot Fwd", mNamedParameters.mRigBootFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd 2", mNamedParameters.mRigFrontQCuFwd2, lrSerialiser);
        SerialiseBehaviourParameters("Rig Underbelly", mNamedParameters.mRigUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Underbelly", mNamedParameters.mRigDropUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Front Q Cu Fwd", mNamedParameters.mRigDropFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Boot Q Cu Fwd", mNamedParameters.mRigDropBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Failsafe", mNamedParameters.mFailsafe, lrSerialiser);
        SerialiseBehaviourParameters("Passenger", mNamedParameters.mPassengerDefault, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown1", mNamedParameters.mLooseAttachmentTakedown1, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown2", mNamedParameters.mLooseAttachmentTakedown2, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown3", mNamedParameters.mLooseAttachmentTakedown3, lrSerialiser);
        SerialiseBehaviourParameters("Fixed Cam Default", mNamedParameters.mFixedDefault, lrSerialiser);
        break;
    case 4:
        SerialiseBehaviourParameters("Aftertouch", mNamedParameters.mAftertouchCamDefault, lrSerialiser);
        SerialiseBehaviourParameters("Aftertouch Crash", mNamedParameters.mAftertouchCrashParams, lrSerialiser);
        SerialiseBehaviourParameters("Crash Debug", mNamedParameters.mCrashDebugParams, lrSerialiser);
        SerialiseBehaviourParameters("HeliCam Default", mNamedParameters.mHeliCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default", mNamedParameters.mGyroCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Front", mNamedParameters.mGyroCamTruckFront, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Left", mNamedParameters.mGyroCamLeft, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Right", mNamedParameters.mGyroCamRight, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Left", mNamedParameters.mGyroCamDefaultSideTruckingLeftParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Right", mNamedParameters.mGyroCamDefaultSideTruckingRightParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Follow", mNamedParameters.mGyroCamFollow, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Always Low", mNamedParameters.mGyroCamAlwaysLowParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown", mNamedParameters.mGyroCamTakedownParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam High", mNamedParameters.mGyroCamHighParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Helicam", mNamedParameters.mGyroCamHelicamParams, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump Left", mNamedParameters.mBystanderJumpLeftParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump2", mNamedParameters.mBystanderJumpParameters2, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Jump From Behind", mNamedParameters.mBystanderJumpFromBehindParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Close", mNamedParameters.mBystanderCloseParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Medium", mNamedParameters.mBystanderMediumParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far", mNamedParameters.mBystanderFarParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far Tall", mNamedParameters.mBystanderFarTallParameters, lrSerialiser);
        SerialiseBehaviourParameters("Rig Bonnet Low Right", mNamedParameters.mRigBonnetLowRight, lrSerialiser);
        SerialiseBehaviourParameters("Rig Rear Q Fwd", mNamedParameters.mRigRearQFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd", mNamedParameters.mRigFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Bwd", mNamedParameters.mRigFrontQBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Rearview", mNamedParameters.mRigFrontRearview, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot View Fwd", mNamedParameters.mRigBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Low Bwd", mNamedParameters.mRigFrontQLowBwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Roof Fwd", mNamedParameters.mRigRoofFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Boot Fwd", mNamedParameters.mRigBootFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd 2", mNamedParameters.mRigFrontQCuFwd2, lrSerialiser);
        SerialiseBehaviourParameters("Rig Underbelly", mNamedParameters.mRigUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Underbelly", mNamedParameters.mRigDropUnderbelly, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Front Q Cu Fwd", mNamedParameters.mRigDropFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Drop Boot Q Cu Fwd", mNamedParameters.mRigDropBootViewFwd, lrSerialiser);
        SerialiseBehaviourParameters("Failsafe", mNamedParameters.mFailsafe, lrSerialiser);
        SerialiseBehaviourParameters("Passenger", mNamedParameters.mPassengerDefault, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown1", mNamedParameters.mLooseAttachmentTakedown1, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown2", mNamedParameters.mLooseAttachmentTakedown2, lrSerialiser);
        SerialiseBehaviourParameters("Loose Attachment Takedown3", mNamedParameters.mLooseAttachmentTakedown3, lrSerialiser);
        SerialiseBehaviourParameters("Fixed Cam Default", mNamedParameters.mFixedDefault, lrSerialiser);
        break;
    case 5:
        SerialiseBehaviourParameters("Aftertouch", mNamedParameters.mAftertouchCamDefault, lrSerialiser);
        SerialiseBehaviourParameters("Aftertouch Crash", mNamedParameters.mAftertouchCrashParams, lrSerialiser);
        SerialiseBehaviourParameters("Crash Debug", mNamedParameters.mCrashDebugParams, lrSerialiser);
        SerialiseBehaviourParameters("HeliCam Default", mNamedParameters.mHeliCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Default", mNamedParameters.mGyroCamDefaultParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Front", mNamedParameters.mGyroCamTruckFront, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Left", mNamedParameters.mGyroCamDefaultSideTruckingLeftParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Truck Right", mNamedParameters.mGyroCamDefaultSideTruckingRightParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Follow", mNamedParameters.mGyroCamFollow, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Always Low", mNamedParameters.mGyroCamAlwaysLowParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown", mNamedParameters.mGyroCamTakedownParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Takedown Zoomed Out", mNamedParameters.mGyroCamTakedownZoomedOutParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam High", mNamedParameters.mGyroCamHighParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam Helicam", mNamedParameters.mGyroCamHelicamParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam DriveBy L", mNamedParameters.mGyroCamDriveByLParams, lrSerialiser);
        SerialiseBehaviourParameters("GyroCam DriveBy R", mNamedParameters.mGyroCamDriveByRParams, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Close", mNamedParameters.mBystanderCloseParameters, lrSerialiser);
        SerialiseBehaviourParameters("Bystander Far", mNamedParameters.mBystanderFarParameters, lrSerialiser);
        SerialiseBehaviourParameters("Rig Bonnet Low Right", mNamedParameters.mRigBonnetLowRight, lrSerialiser);
        SerialiseBehaviourParameters("Rig Rear Q Fwd", mNamedParameters.mRigRearQFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Cu Fwd", mNamedParameters.mRigFrontQCuFwd, lrSerialiser);
        SerialiseBehaviourParameters("Rig Front Q Bwd", mNamedParameters.mRigFrontQBwd, lrSerialiser);
        SerialiseBehaviourParameters("Failsafe", mNamedParameters.mFailsafe, lrSerialiser);
        SerialiseBehaviourParameters("Fixed Cam Default", mNamedParameters.mFixedDefault, lrSerialiser);
        SerialiseBehaviourParameters("Rotate About Vehicle Default", mNamedParameters.maLookAroundCarCamParameters, lrSerialiser);
        SerialiseBehaviourParameters("Spiralling Deathcam Default", mNamedParameters.maSpirallingDeathcamParameters, lrSerialiser);
        SerialiseBehaviourParameters("Road Runner Default", mNamedParameters.mRoadRunnerDefault, lrSerialiser);
        break;
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
// TestbedSetupSerialiser / BehaviourParameterNamingSerialiser visitors: register the block's
// testbed activation under its debug name / give the block its walk name.
// ----------------------------------------------------------------------------
template<class T>
void TestbedSetupSerialiser::Serialise(const char* /*lpcName*/, T& lrParams)
{
    mpDebugComponent->RegisterFunction(&ArbStateTestbed::GenericActivateCam, &lrParams, "Testbed",
                                       lrParams.GetDebugName());
}

template<class T>
void BehaviourParameterNamingSerialiser::Serialise(const char* lpcName, T& lrParams)
{
    lrParams.SetDebugName(lpcName);
}

// ----------------------------------------------------------------------------
// SerialiseBehaviourParameters<TSerialiser> -- dispatch one bank block on its type tag to
// lrSerialiser.Serialise(lpcName, <the block as its concrete Parameters type>).
// ----------------------------------------------------------------------------
template<class TSerialiser>
void SerialiseBehaviourParameters(const char* lpcName, Behaviour::Parameters& lrParameters,
                                  TSerialiser& lrSerialiser)
{
    switch (lrParameters.GetType())
    {
    case eBehaviourGameplayExternal:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourGameplayExternal::Parameters&>(lrParameters));
        break;
    case eBehaviourGameplayBumper:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourGameplayBumper::Parameters&>(lrParameters));
        break;
    case eBehaviourRig:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourRig::Parameters&>(lrParameters));
        break;
    case eBehaviourBystanderCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourBystanderCam::Parameters&>(lrParameters));
        break;
    case eBehaviourHeliCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourHeliCam::Parameters&>(lrParameters));
        break;
    case eBehaviourPassengerCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourPassengerCam::Parameters&>(lrParameters));
        break;
    case eBehaviourGyroCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourGyroCam::Parameters&>(lrParameters));
        break;
    case eBehaviourAftertouchCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourAftertouchCam::Parameters&>(lrParameters));
        break;
    case eBehaviourAftertouchCrash:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourAftertouchCrash::Parameters&>(lrParameters));
        break;
    case eBehaviourFailsafe:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourFailsafe::Parameters&>(lrParameters));
        break;
    case eBehaviourLooseAttachment:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourLooseAttachment::Parameters&>(lrParameters));
        break;
    case eBehaviourFixedCam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourFixedCam::Parameters&>(lrParameters));
        break;
    case 18:   // eBehaviourRotateAboutVehicle
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourRotateAboutVehicle::Parameters&>(lrParameters));
        break;
    case BehaviourSpirallingDeathcam::eBehaviourSpirallingDeathcam:
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourSpirallingDeathcam::Parameters&>(lrParameters));
        break;
    case 16:   // eBehaviourRoadRunner
        lrSerialiser.Serialise(lpcName, static_cast<BehaviourRoadRunner::Parameters&>(lrParameters));
        break;
    default:
    {
        CgsDev::Assert::BeginAssert();
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Params type needs to be added to Serialise: ";
        lStrStream << lrParameters.GetDebugName();
        CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
        break;
    }
    }
}

// ----------------------------------------------------------------------------
// BehaviourParameterBank::Construct -- the latched car key and the two gameplay blocks, the record,
// then the naming pass that gives every block of the current walk its debug name. The bank leaves
// both gameplay blocks' mbIsValid false; only Parameters::Set raises them.
//
// [FLAG, PC-only] the ZeroBlock calls are not console behaviour: the console leaves those bytes
// at whatever the manager's storage held. They keep every PC consumer off indeterminate storage.
// ----------------------------------------------------------------------------
void BehaviourParameterBank::Construct()
{
    muVersion.muVersion = 0;
    ZeroBlock(maReservedBankHead, sizeof(maReservedBankHead));
    ZeroBlock(&mGameplayExternalCameraParamsForCar, sizeof(mGameplayExternalCameraParamsForCar));
    ZeroBlock(&mGameplayBumperCameraParamsForCar, sizeof(mGameplayBumperCameraParamsForCar));
    ZeroBlock(&mNamedParameters.mFailsafe, sizeof(mNamedParameters.mFailsafe));

    mxGameplayCameraCarAttribsKey = 0;
    mGameplayExternalCameraParamsForCar.Construct();
    mGameplayBumperCameraParamsForCar.Construct();

    mNamedParameters.Construct();

    BehaviourParameterNamingSerialiser lSerialiser;
    Serialise(lSerialiser);
}

// ----------------------------------------------------------------------------
// The camera tweaker's "Save" / "Load" actions: the whole bank to / from "d:\\camera.txt".
// ----------------------------------------------------------------------------
void BehaviourParameterBank::LoadParameters(void* lpVoid)
{
    BehaviourParameterBank* lpBehaviourParameterBank = static_cast<BehaviourParameterBank*>(lpVoid);

    TextFileReadSerialiser lrSerialiser;
    lrSerialiser.Construct("d:\\camera.txt");
    lpBehaviourParameterBank->Serialise(lrSerialiser);
    lrSerialiser.Destruct();
}

void BehaviourParameterBank::SaveParameters(void* lpVoid)
{
    BehaviourParameterBank* lpBehaviourParameterBank = static_cast<BehaviourParameterBank*>(lpVoid);

    TextFileWriteSerialiser lrSerialiser;
    lrSerialiser.Construct("d:\\camera.txt");
    lpBehaviourParameterBank->Serialise(lrSerialiser);
    lrSerialiser.Destruct();
}

template void SerialiseBehaviourParameters<DebugMenuSerialiser>(const char*, Behaviour::Parameters&, DebugMenuSerialiser&);
template void SerialiseBehaviourParameters<TextFileReadSerialiser>(const char*, Behaviour::Parameters&, TextFileReadSerialiser&);
template void SerialiseBehaviourParameters<TextFileWriteSerialiser>(const char*, Behaviour::Parameters&, TextFileWriteSerialiser&);
template void SerialiseBehaviourParameters<TestbedSetupSerialiser>(const char*, Behaviour::Parameters&, TestbedSetupSerialiser&);
template void SerialiseBehaviourParameters<BehaviourParameterNamingSerialiser>(const char*, Behaviour::Parameters&, BehaviourParameterNamingSerialiser&);
template void BehaviourParameterBank::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourParameterBank::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourParameterBank::Serialise<TestbedSetupSerialiser>(TestbedSetupSerialiser&);
template void BehaviourParameterBank::Serialise<BehaviourParameterNamingSerialiser>(BehaviourParameterNamingSerialiser&);

} // namespace Camera
} // namespace BrnDirector
