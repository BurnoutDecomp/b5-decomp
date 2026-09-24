// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.cpp
//
// BrnDirector::Camera::BehaviourBystanderCam -- the behaviour's own bodies, transcribed from the
// ARTIST X360 asm (the DWARF homes every one of them in this file):
//   Parameters::Construct  (cpp:186) @0x821F9A00
//   Construct              (cpp:221) @0x822438E8
//   Prepare                (cpp:251) @0x821F9AD0
//   Update                 (cpp:270) @0x82243C80
//   GetCollisionPolicy     (cpp:396) @0x821F9B28
//   SetupTweaker           (cpp:410) @0x821F9B30
//   GetName                (cpp:435) @0x821F9C78
// SetParameters (h:213, @0x821F3F10), SetTarget (h:250, @0x821F3F80) and
// SetPerceivedDistanceModificationFactor (h:268) are header inlines. The Parameters::Serialise<S>
// visitor lives in the partfile BrnBehaviourBystanderCamSerialise.cpp, because its serialisers are
// not in the exe's link.
//
// ⭐ 2026-09-24 (FX-DIRECTOR): these bodies are what turn this class from a hollow shell into a real
// Camera::Behaviour -- see the banner in the header for the crash they end. The file used to reach
// every foreign object through a `namespace detail` layer of ~30 free-function shims that were
// declared and never defined anywhere, which is why it could not be mounted. Every one of them is
// now the real callee: VehicleRef::IsValid/Get (through Behaviour::VehicleRef::GetVehicle /
// GetTransform), PositionFinder::FindPosition/Update, Looker::Update, CameraShake::Update,
// Behaviour::Fail / SetCantSwitchToMeNow, Timestep::Get, Camera::SetTransform /
// ValidateTransformWithDebugInfo, Tweaker::Construct/AddMapping.
//
// The two impact controllers the DWARF also homes here (ImpactSlomoController::Update @0x82227230,
// ImpactShakeController::Update @0x82243720) stay in the mounted partfile
// BehaviourBystanderCamImpactControllers.cpp, written against the real types; the crash arbitrator
// state embeds both by value and they share no member with the behaviour.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"               // Utils::TargetOutsideRange / TargetWillExceedRangeInXSecs
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"          // Utils::Tweaker (SetupTweaker)
#include "rw/math/vpu/vector3_operation.h"                              // MagnitudeSquared / GetVector3_YAxis
#include "rw/math/vpu/matrix44affine_operation.h"                       // TransformPoint

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // ---- Parameters::Construct's authored defaults (@0x821F9A00, each read at its load site) ----
    const f32 KF_DEFAULT_SHAKE_XY_MAGNITUDE_DEGS      = 0.0f;    // +0x08  flt_82001CC0 (over the seed's 0.06)
    const f32 KF_DEFAULT_SHAKE_WOBBLE_CENTERING       = 0.25f;   // +0x14  flt_82003F40 (over the seed's 0.11)
    const f32 KF_DEFAULT_SHAKE_XY_WOBBLE_DEGS         = 1.0f;    // +0x10  flt_82001C98 (over the seed's 1.15)
    const f32 KF_DEFAULT_VELOCITY_INFLUENCE           = 0.5f;    // +0x7C  flt_82001DA0
    const f32 KF_DEFAULT_MAX_INITIAL_DISTANCE_KM      = 40.0f;   // +0x80  flt_82004D0C
    const f32 KF_DEFAULT_DISTANCE_FOR_FAIL_KM         = 80.0f;   // +0x84  flt_82004A18
    const f32 KF_DEFAULT_TARGET_SPACE_X               = 2.0f;    // +0x88  flt_82001D9C
    const f32 KF_DEFAULT_TARGET_SPACE_YZ              = 0.0f;    // +0x8C / +0x90  flt_82001CC0 (f7)
    const f32 KF_DEFAULT_HEIGHT                       = 1.0f;    // +0x94  flt_82001C98 (f6)

    // ---- Construct ----
    const f32 KF_DEFAULT_PERCEIVED_DISTANCE_FACTOR    = 1.0f;    // +0x358 flt_82001C98

    // ---- Update's constants ----
    const f32 KF_KM_TO_M                              = 1000.0f; // f30    flt_82009E10 (0x447A0000)
    const f32 KF_SHAKE_SCALE                          = 1.0f;    // f2     flt_82001C98
    const f32 KF_LEAVE_FRAME_LOOKAHEAD_SECS           = 0.5f;    // lfSecs flt_82001DA0

    // The validity-account flags Update raises (names from the account's recovered table).
    const s32 KI_FAILED_COULDNT_FIND_ROADSIDE         = 6;       // `li r5, 6` into Behaviour::Fail
    const s32 KI_FAILED_POSITION_TOO_FAR_FROM_SUBJECT = 7;       // `li r5, 7` into Behaviour::Fail
    const s32 KI_FAILED_INVALID_VEHICLE_REF           = 11;      // the inlined Fail: `ori 0x800` on camera +0x138
    const s32 KI_NOCUTTO_FINDING_POSITION             = 14;      // `ori 0x4000` on camera +0x138
    const s32 KI_NOCUTTO_SUBJECT_ABOUT_TO_LEAVE_FRAME = 15;      // `ori 0x8000` on camera +0x138
    const s32 KI_NOCUTTO_SUBJECT_OCCLUDED             = 17;      // `oris 2` (0x20000) on camera +0x138

    // The camera-state flag Update raises while the behaviour has not failed (`ori r11, r11, 2` on
    // camera +0x140, 0x82243FD8) -- CameraState::E_FLAG_VALID, the same bit every live behaviour raises.
    const s32 KI_CAMERA_STATE_FLAG_VALID_BIT          = 1 << CameraState::E_FLAG_VALID;

    // ---- SetupTweaker ----
    const f32 KF_TWEAK_TRANSLATION_SPEED              = 0.05f;   // lfTranslationSpeed (cpp:414) flt_820047C8
    const f32 KF_TWEAK_TRANSLATION_SPEED_X            = -0.05f;  // the "Rig X" scale, folded   flt_82004E3C
}

// ----------------------------------------------------------------------------
// Parameters::Construct @0x821F9A00 (cpp:186). The base's Construct, the type tag, the shake block's
// own Construct (the inlined 0.11 / 1.15 / 0.06 / 0.0 seed) re-tuned on three of its four floats,
// the looker block's own Construct (`bl Looker::Parameters::Construct` on this+0x18) with zoom turned
// on, then the bystander scalars.
// ----------------------------------------------------------------------------
void BehaviourBystanderCam::Parameters::Construct()
{
    Behaviour::Parameters::Construct();                                   // stw 0, 4(r8)
    mType = eBehaviourBystanderCam;                                       // stw 5, 0(r8)

    mShakeParams.Construct();                                             // +0x14 / +0x10 / +0x08 / +0x0C
    mShakeParams.mfXYShakeMagnitudeDegs  = KF_DEFAULT_SHAKE_XY_MAGNITUDE_DEGS;   // stfs f7,  8(r8)
    mShakeParams.mfWobbleCenteringFactor = KF_DEFAULT_SHAKE_WOBBLE_CENTERING;    // stfs f11, 0x14(r8)
    mShakeParams.mfXYWobbleMagnitudeDegs = KF_DEFAULT_SHAKE_XY_WOBBLE_DEGS;      // stfs f6,  0x10(r8)

    mLookerParams.Construct();                                            // bl 0x821F8D80
    mLookerParams.mbUseZoom = true;                                       // stb 1, 0x77(r8)

    mfVelocityInfluenceOnPosition           = KF_DEFAULT_VELOCITY_INFLUENCE;        // stfs 0x7C
    mfMaxInitialDistanceKM                  = KF_DEFAULT_MAX_INITIAL_DISTANCE_KM;   // stfs 0x80
    mfDistanceForFailKM                     = KF_DEFAULT_DISTANCE_FOR_FAIL_KM;      // stfs 0x84
    mfTargetSpaceX                          = KF_DEFAULT_TARGET_SPACE_X;            // stfs 0x88
    mfTargetSpaceY                          = KF_DEFAULT_TARGET_SPACE_YZ;           // stfs 0x8C
    mfTargetSpaceZ                          = KF_DEFAULT_TARGET_SPACE_YZ;           // stfs 0x90
    mfHeight                                = KF_DEFAULT_HEIGHT;                    // stfs 0x94
    mbUseTargetSpaceInsteadOfPositionFinder = false;                                // stb 0, 0x98
    mbUseRangeTesting                       = true;                                 // stb 1, 0x99
}

// ----------------------------------------------------------------------------
// Construct @0x822438E8 (cpp:221). Every block is an inlined Construct:
//   * Behaviour::Construct -- +0x04 = 0, +0x08..+0x0C = 0, +0x10 = 0;
//   * PositionFinder::Construct over +0x60 -- stb 0 +0x90, stb 0 +0x91, stb 1 +0x92;
//   * CameraShake::Construct over +0xA0 -- four stfs 0.0;
//   * Looker::Construct over +0xB0 -- 0.0 +0xB0, 0.2 +0xC0 (flt_82004744), 1/1/1/0 +0xCC..+0xCF;
//   * CgsNumeric::Random::Construct over +0x310 -- the default seed 0xC87CD8C91AD0891B, slot 0 = 1.0f,
//     seven AddRandomFloatToBuffer steps of the 0x5851F42D4C957F2D LCG, the index bump;
//   * Matrix44Affine::SetIdentity (the DWARF's call) -- rows (1,0,0,0) (0,1,0,0) (0,0,1,0) (0,0,0,0)
//     built on the stack and stored to +0x20..+0x50;
//   * VisibilityCollisionPolicy::Construct over +0xD0 (the same store set BehaviourFixedCam's shows);
//   * mfPerceivedDistanceModificationFactor = 1.0 (+0x358);
//   * VisibilityCollisionPolicy::SetTestLookingAt(false) -- `stb 0, 0x270` AFTER the policy's own
//     `stb 1` to policy +0x1A0;
//   * mbSetup = 1 (+0x35C), mbGotPosition = 0 (+0x35D);
//   * VehicleRef::Construct (`stb 0, 0x34C`) then VehicleRef::SetToPlayer (the DWARF's call:
//     +0x34C = 1, +0x340 = E_PLAYER_CAR, +0x348 = 0, +0x344 = -1).
// mpParameters (+0x350) and mfSquaredDistanceToTarget (+0x354) are not written.
// ----------------------------------------------------------------------------
void BehaviourBystanderCam::Construct()
{
    Behaviour::Construct();
    mPositionFinder.Construct();
    mShake.Construct();
    mLooker.Construct();
    mRandom.Construct();
    mTransform.SetIdentity();
    mCollisionPolicy.Construct();
    mfPerceivedDistanceModificationFactor = KF_DEFAULT_PERCEIVED_DISTANCE_FACTOR;
    mCollisionPolicy.SetTestLookingAt(false);
    mbSetup       = true;
    mbGotPosition = false;
    mTarget.Construct();
    mTarget.SetToPlayer();
}

// ----------------------------------------------------------------------------
// Prepare @0x821F9AD0 (cpp:251): `stb 1, 8(r3)` (SetPrepared), assert mbSetup (cpp:256, the text
// "mbSetup" at 0x82004DD0), `li r3, 1`.
// ----------------------------------------------------------------------------
bool BehaviourBystanderCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetPrepared();
    CGS_ASSERT(mbSetup, "mbSetup");   // cpp:256
    return true;
}

// ----------------------------------------------------------------------------
// Update @0x82243C80 (cpp:270). The subject is mTarget, re-resolved against the shared info's
// AllVehicleData (+0x5BC) at every use, as the console does (VehicleRef::Get @0x822335A0 per read).
//   0x82243CA8  assert mpParameters (cpp:272, "mpParameters != NULL").
//   0x82243CE0  !VehicleRef::IsValid -> the inlined Fail(11) "Invalid vehicle ref"; return true.
//   0x82243D20  the inlined VisibilityCollisionPolicy::SetTarget: the subject's transform (+0x1F0),
//               its AABB (+0x4A0) and entity id (+0x3C8).
//   0x82243DF0  if (!mbGotPosition || the tweaker is attached) -- (re)plant the camera:
//                 the inlined SetCantSwitchToMeNow(14) "NoCutTo: Finding position";
//                 mbUseTargetSpaceInsteadOfPositionFinder -> mbGotPosition = 1 and the camera sits at
//                   TransformPoint(subject transform, (mfTargetSpaceX, Y, Z)) (the vmaddfp cascade);
//                 else -> seed the PositionFinder once (the subject's position, its velocity scaled
//                   by mfVelocityInfluenceOnPosition), poll it; nothing found -> Fail(6) "Couldn't
//                   find roadside"; found but further than mfMaxInitialDistanceKM * 1000 from the
//                   subject -> Fail(7); else the camera sits at the found point, mbGotPosition = 1,
//                   lifted by GetVector3_YAxis() (0x82181510) * mfHeight.
//   0x82243FC8  not failed -> camera state flag E_FLAG_VALID.
//   0x82243FE0  a copy of mLookerParams whose mfDesiredPerceivedDistance is scaled by
//               mfPerceivedDistanceModificationFactor; dt = Timestep::Get(+0x550, meTimestepType).
//   0x82244028  the camera takes mTransform (SetTransform), ValidateTransformWithDebugInfo; the
//               Looker turns/zooms it onto the subject (mRandom passed BY VALUE -- the six `ld`s);
//               mTransform takes the result back; the shake wobbles the camera (scale 1.0).
//   0x8224411C  mfSquaredDistanceToTarget = |subject position - camera position|^2.
//   0x82244130  failed -> return true.
//   0x82244158  mbUseRangeTesting and the subject further than mfDistanceForFailKM * 1000 -> Fail(7).
//   0x822441D0  the policy's visibility interrupted -> SetCantSwitchToMeNow(17) "Subject occluded";
//               else mbUseRangeTesting and the subject will be out of range in 0.5 s ->
//               SetCantSwitchToMeNow(15) "Subject about to leave frame".
//   return true.
// ----------------------------------------------------------------------------
bool BehaviourBystanderCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    using namespace rw::math::vpu;

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // cpp:272

    if (!mTarget.IsValid(*lrInfo.GetWorld()))
    {
        Fail(lrCamera, KI_FAILED_INVALID_VEHICLE_REF);
        return true;
    }

    mCollisionPolicy.SetTarget(mTarget.GetTransform(lrInfo), mTarget.GetVehicle(lrInfo).mAABB,
                               CgsSceneManager::EntityId(
                                   mTarget.GetVehicle(lrInfo).mRaceCarState.mEntityId.muValue));

    if (!mbGotPosition || IsTweakerAttached())
    {
        SetCantSwitchToMeNow(lrCamera, KI_NOCUTTO_FINDING_POSITION);

        if (mpParameters->mbUseTargetSpaceInsteadOfPositionFinder)
        {
            mbGotPosition = true;
            const Vector3 lTargetSpacePosition = { mpParameters->mfTargetSpaceX, mpParameters->mfTargetSpaceY,
                                                   mpParameters->mfTargetSpaceZ, 0.0f };
            mTransform.wAxis = TransformPoint(mTarget.GetTransform(lrInfo), lTargetSpacePosition);
        }
        else
        {
            if (!mPositionFinder.IsInitialised())
            {
                const Vector3 lTargetVelocity = mTarget.GetVehicle(lrInfo).mRaceCarState.mLinearVelocity;
                mPositionFinder.FindPosition(mTarget.GetTransform(lrInfo).wAxis,
                                             lTargetVelocity * mpParameters->mfVelocityInfluenceOnPosition);
            }

            mPositionFinder.Update(lrInfo);

            if (!mPositionFinder.HasFoundPosition())
            {
                Fail(lrCamera, KI_FAILED_COULDNT_FIND_ROADSIDE);
                return true;
            }

            if (Utils::TargetOutsideRange(mPositionFinder.GetPosition(), mTarget.GetTransform(lrInfo).wAxis,
                                          mpParameters->mfMaxInitialDistanceKM * KF_KM_TO_M))
            {
                Fail(lrCamera, KI_FAILED_POSITION_TOO_FAR_FROM_SUBJECT);
                return true;
            }

            mTransform.wAxis = mPositionFinder.GetPosition();
            mbGotPosition    = true;
            mTransform.wAxis = mTransform.wAxis + GetVector3_YAxis() * mpParameters->mfHeight;
        }
    }

    if (!HasFailed())
    {
        lrCamera.mState_uFlags |= KI_CAMERA_STATE_FLAG_VALID_BIT;
    }

    Utils::Looker::Parameters lLookerParams = mpParameters->mLookerParams;          // memcpy 0x64
    lLookerParams.mfDesiredPerceivedDistance *= mfPerceivedDistanceModificationFactor;
    const f32 lfTimestep = lrInfo.GetTimestep(GetTimestepType());

    lrCamera.SetTransform(mTransform);
    lrCamera.ValidateTransformWithDebugInfo();

    mLooker.Update(VecFloat(lfTimestep), mRandom, lLookerParams, lrCamera, mTarget.GetTransform(lrInfo),
                   mTarget.GetVehicle(lrInfo).mRaceCarState.mLinearVelocity, mTarget.GetVehicle(lrInfo).mAABB);
    mTransform = lrCamera.mTransform;

    mShake.Update(lrCamera.mTransform, mpParameters->mShakeParams, mRandom, lfTimestep, KF_SHAKE_SCALE);

    mfSquaredDistanceToTarget = MagnitudeSquared(mTarget.GetTransform(lrInfo).wAxis - mTransform.wAxis);

    if (HasFailed())
    {
        return true;
    }

    if (mpParameters->mbUseRangeTesting &&
        Utils::TargetOutsideRange(mTransform.wAxis, mTarget.GetTransform(lrInfo).wAxis,
                                  mpParameters->mfDistanceForFailKM * KF_KM_TO_M))
    {
        Fail(lrCamera, KI_FAILED_POSITION_TOO_FAR_FROM_SUBJECT);
        return true;
    }

    if (mCollisionPolicy.IsVisibilityInterrupted())
    {
        SetCantSwitchToMeNow(lrCamera, KI_NOCUTTO_SUBJECT_OCCLUDED);
    }
    else if (mpParameters->mbUseRangeTesting &&
             Utils::TargetWillExceedRangeInXSecs(mTransform.wAxis, mTarget.GetTransform(lrInfo).wAxis,
                                                 mTarget.GetVehicle(lrInfo).mRaceCarState.mLinearVelocity,
                                                 mpParameters->mfDistanceForFailKM * KF_KM_TO_M,
                                                 KF_LEAVE_FRAME_LOOKAHEAD_SECS))
    {
        SetCantSwitchToMeNow(lrCamera, KI_NOCUTTO_SUBJECT_ABOUT_TO_LEAVE_FRAME);
    }

    return true;
}

// ----------------------------------------------------------------------------
// GetCollisionPolicy @0x821F9B28 (cpp:396): `addi r3, r3, 0xD0 ; blr`.
// ----------------------------------------------------------------------------
CollisionPolicy* BehaviourBystanderCam::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

// ----------------------------------------------------------------------------
// SetupTweaker @0x821F9B30 (cpp:410). Tweaker::Construct, assert mpParameters (cpp:416, text
// "mpParameters" at 0x82004DB0), then three inlined constant-scale Tweaker::AddMapping calls (each
// with its BrnCameraTweaker.cpp:146 "lpfVariableToTweak != NULL" assert), in this order:
//   "Rig X" (0x82004E34) -> &mfTargetSpaceX, -0.05, maAxisMapping[NORMAL][LEFT_STICK_X]   (+0x00)
//   "Rig Y" (0x82004E2C) -> &mfTargetSpaceY,  0.05, maAxisMapping[NORMAL][LOWER_TRIGGERS] (+0x78)
//   "Rig Z" (0x82004E24) -> &mfTargetSpaceZ,  0.05, maAxisMapping[NORMAL][LEFT_STICK_Y]   (+0x14)
// The console tweaks the shared parameter block in place, through the const pointer.
// ----------------------------------------------------------------------------
void BehaviourBystanderCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();

    CGS_ASSERT(mpParameters != 0, "mpParameters");   // cpp:416

    Parameters* lpParams = const_cast<Parameters*>(mpParameters);
    lrTweaker.AddMapping("Rig X", &lpParams->mfTargetSpaceX, KF_TWEAK_TRANSLATION_SPEED_X,
                         Utils::Tweaker::E_AXIS_LEFT_STICK_X, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Rig Y", &lpParams->mfTargetSpaceY, KF_TWEAK_TRANSLATION_SPEED,
                         Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Rig Z", &lpParams->mfTargetSpaceZ, KF_TWEAK_TRANSLATION_SPEED,
                         Utils::Tweaker::E_AXIS_LEFT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);
}

// ----------------------------------------------------------------------------
// GetName @0x821F9C78 (cpp:435): the literal at 0x82004E40.
// ----------------------------------------------------------------------------
const char* BehaviourBystanderCam::GetName() const
{
    return "BehaviourBystanderCam";
}

} // namespace Camera
} // namespace BrnDirector
