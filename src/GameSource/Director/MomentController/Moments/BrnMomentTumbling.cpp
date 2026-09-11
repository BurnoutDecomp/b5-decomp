#include "GameSource/Director/MomentController/Moments/BrnMomentTumbling.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                         // Vector3 operator-/*/+ + Dot
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"   // AllVehicleData (used mask + race cars)

// BrnDirector::MomentTumbling -- reconstructed from the console executable
// (home file BrnMomentTumbling.cpp; member/parameter names verbatim
// from the declarations).
//
// Bodied here (7 ledger functions):
//   Construct   Update   Release
//   SetParameters   GetName
//   SignalIsGoodTimeToPlant
//   SetGyroCamParameters

namespace BrnDirector
{

namespace
{
    // Read-only-data constants: the per-frame angular-velocity smoothing factor
    // and the tumble speed-squared eligibility threshold (the declared
    // statics kfTumbleSensitivity / kfTumbleStartThreshold. The third static,
    // kfTumbleStopThreshold, is read by nothing this TU bodies).
    const f32 KF_TUMBLE_SENSITIVITY     = 0.1f;
    const f32 KF_TUMBLE_START_THRESHOLD = 200.0f;   //  (|v|^2)

    // How long a lapsed FOLLOW/LEAD tumble may keep running before it gives up
    // the switch gate (the state-3 running-time compare).
    const f32 KF_TUMBLE_LAPSED_TIMEOUT = 0.5f;

    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING       = 18;
    const u32 KU_HEAD_FLAG_ALLOCATED       = 19;
    const u32 KU_HEAD_FLAG_PREPARING       = 20;
    const u32 KU_HEAD_FLAG_LAPSED          = 21;
    const u32 KU_HEAD_FLAG_TIMED_OUT       = 22;
    const u32 KU_HEAD_FLAG_INHIBITED       = 23;
    const u32 KU_HEAD_FLAG_NOT_SWITCHABLE  = 24;
    const u32 KU_HEAD_FLAG_VALID           = 28;   // (not raised here; family doc)
    const u32 KU_HEAD_FLAG_NOT_RELEASABLE  = 30;

    // The camera-state CURRENT flag the valid body raises while framing the
    // PLAYER's own tumble (SetFlag(9) -- mask 0x200; role not yet recovered).
    const u32 KU_STATE_FLAG_PLAYER_TUMBLE = 9;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; console shared-info offsets in comments). ----
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo);        // +1284 byte 249
    bool MomentSharedInfo_WasTakedown(const void* lpSharedInfo);             // +1284 byte 218
    bool MomentSharedInfo_IsCrashCameraBlocked(const void* lpSharedInfo);    // +1284 byte 449
    bool MomentSharedInfo_IsCrashReplayDisabled(const void* lpSharedInfo);   // +1284 byte 284
    s32  MomentSharedInfo_GetTakedownVictimIndex(const void* lpSharedInfo);  // +1284 word +224
    bool MomentSharedInfo_GetForceFlag1320(const void* lpSharedInfo);        // +1320 byte

    // The two Vector3 lanes embedded DIRECTLY in the shared record (this moment
    // lvx128s them off the record base, unlike the +1292-block lanes its
    // stationary-crash sibling reads):
    const rw::math::vpu::Vector3& MomentSharedInfo_GetPlayerVelocity(const void* lpSharedInfo);         // +816
    const rw::math::vpu::Vector3& MomentSharedInfo_GetPlayerAngularVelocity(const void* lpSharedInfo);  // +832

    const AllVehicleData* MomentSharedInfo_GetAllVehicleData(const void* lpSharedInfo);   // +1288

    // The named camera parameter record SetGyroCamParameters picks its gyro block out of.
    const NamedParameters* MomentSharedInfo_GetNamedBehaviourParams(const void* lpSharedInfo);

    // ⭐ RETIRED 2026-08-23 (moment-camera wave). This used to be a DECLARATION-ONLY
    // `VehicleInfo_GetVelocity(const VehicleInfo&)` shim -- an unresolved external AND an
    // undeclared type (`VehicleInfo` is not a name in BrnDirector; the real record is
    // BrnDirector::Camera::VehicleInfo), which is why this TU never compiled.
    // The "+816" lane it stood for IS a named member: Camera::VehicleInfo::mRaceCarState
    // .mLinearVelocity (BrnVehicleEvents.h RaceCarState @816). Both call sites now read it
    // by name, so nothing is stubbed and nothing is guessed.
}
using namespace detail;

// The inlined base Moment::Construct, the gyro handle
// clear, and the latch seeds (mbLookingAtTakedown and mfRunningTime are seeded
// per-allocation in Update, not here).
void MomentTumbling::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera)
    mGyroCam.Clear();      // the console build zeroes the five handle fields inline
    mbUseLeftForThisCrash  = false;
    mbUseRightForThisCrash = false;
    mbFirstTryThisCrash    = true;
    mbTryTrucking          = true;
    mbTryLeft              = true;
    mpParameters           = 0;
}

// The inlined guarded handle Release, the gate clears,
// the searching head bit, then park at INACTIVE (state 0 -- this moment's
// distinct Release target).
bool MomentTumbling::Release()
{
    mGyroCam.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_INACTIVE);
    return true;
}

// Adopt the tuning record (no type assert in the console build).
void MomentTumbling::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

const char* MomentTumbling::GetName() const
{
    return "MomentTumbling";
}

// Push the subtype-selected gyro parameter block onto the rig this moment has
// just allocated. Five subtype arms, each naming one block of
// the named camera parameter record; the block names and the subtype names agree
// (see the RECORD MAP banner in BrnBehaviourParameterBank.h for how they are pinned).
//
// The TRUCKING_SIDE arm is the only one with state: it alternates left / right block
// across allocations and latches which side this crash settled on, so a crash that
// re-allocates the rig keeps the side it started with rather than flip-flopping.
//   - already latched left            -> the left block, no state change
//   - already latched right           -> the right block, no state change
//   - neither latched: mbTryLeft picks the block, that side's latch goes up, and
//     mbTryLeft flips so the NEXT crash starts on the other side
// The alternation is deliberately not collapsed into the two latch tests: the console
// re-reads mbUseLeftForThisCrash inside the right-latched arm, and the flip is a
// separate statement that runs on the un-latched path only.
void MomentTumbling::SetGyroCamParameters(const void* lSharedInfo)
{
    const NamedParameters& lrNamed = *MomentSharedInfo_GetNamedBehaviourParams(lSharedInfo);

    switch (mpParameters->meSubType)
    {
    case Parameters::E_SUBTYPE_TRUCKING_SIDE:
        if (!mbUseLeftForThisCrash)
        {
            if (mbUseRightForThisCrash)
            {
                mGyroCam.GetBehaviour()->SetParameters(
                    &lrNamed.mGyroCamDefaultSideTruckingRightParams);
                return;
            }
            if (!mbTryLeft)
            {
                mGyroCam.GetBehaviour()->SetParameters(
                    &lrNamed.mGyroCamDefaultSideTruckingRightParams);
                mbUseRightForThisCrash = true;
            }
            else
            {
                mGyroCam.GetBehaviour()->SetParameters(
                    &lrNamed.mGyroCamDefaultSideTruckingLeftParams);
                mbUseLeftForThisCrash = true;
            }
            mbTryLeft = !mbTryLeft;
            return;
        }
        mGyroCam.GetBehaviour()->SetParameters(&lrNamed.mGyroCamDefaultSideTruckingLeftParams);
        return;

    case Parameters::E_SUBTYPE_TRUCKING_FRONT:
        mGyroCam.GetBehaviour()->SetParameters(&lrNamed.mGyroCamTruckFront);
        return;

    case Parameters::E_SUBTYPE_FOLLOW:
        mGyroCam.GetBehaviour()->SetParameters(&lrNamed.mGyroCamFollow);
        return;

    case Parameters::E_SUBTYPE_LEAD:
        mGyroCam.GetBehaviour()->SetParameters(&lrNamed.mGyroCamDefaultParams);
        return;

    case Parameters::E_SUBTYPE_SIDE:
        mGyroCam.GetBehaviour()->SetParameters(&lrNamed.mGyroCamLeft);
        return;

    default:
        CGS_ASSERT(false, "invalid subtype");   //  (non-gating)
        return;
    }
}

// On a LEAD-subtype tumble, raise the gyro rig's plant
// request pair (behaviour +0x630/+0x631).
void MomentTumbling::SignalIsGoodTimeToPlant()
{
    CGS_ASSERT(IsValid(), "IsValid()");   //  (non-gating)
    if (mpParameters->meSubType == Parameters::E_SUBTYPE_LEAD)
    {
        mGyroCam.GetBehaviour()->SignalGoodTimeToPlant();
    }
}

// Every frame FIRST smooth the tracked angular velocity:
//   mSmoothedAngularVelocity += (sharedAngularVelocity - mSmoothedAngularVelocity)
//                               * KF_TUMBLE_SENSITIVITY
// (the console build runs this as a full-vector multiply-add, expressed here with the vpu
// vector operators -- the RaceIntro vector-op precedent). Then:
//   SEARCHING        while not crashing, re-arm the per-crash latches. Two
//                    triggers: the TAKEDOWN one (the takedown byte &&
//                    Parameters::mbTakedownMoment && the victim is a live race
//                    car (used-mask bit, with the BitArray bound tripwire) whose
//                    |velocity|^2 clears the tumble threshold) and the CRASH one
//                    (crashing && not blocked && the player's |velocity|^2
//                    clears it && Parameters::mbCrashMoment && replays enabled),
//                    OR the force flag. Allocate the gyro cam, push the
//                    subtype's parameters (SetGyroCamParameters, its own TU),
//                    attach it to the victim on a pure-takedown trigger, and
//                    enter FOUND_PREPARING.
//   FOUND_PREPARING  switchable -> zero the running time, consume the
//                    first-try latch, VALID (and run its body this frame);
//                    failed -> (re-arm the side latches while first-try) release
//                    the handle and back to SEARCHING; else hold (bit 20).
//   VALID            mirror the produced camera (raising current flag 9 while
//                    framing the player's own tumble); re-evaluate the keep
//                    condition (takedown- or crash-keep, sans the parameter
//                    gates); publish the rig's own switch gates (bits 30/24);
//                    while lapsed, integrate the running time (bit 21) and --
//                    for FOLLOW/LEAD subtypes past 0.5s -- give up the switch
//                    gate (bit 22); force the tumble motion blur (0.25 on
//                    FOLLOW/LEAD, else 0) and drop the moment when the rig
//                    fails.
void MomentTumbling::Update(f32 lfTimeStep, void* lrBehaviourController,
                            const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   //  (non-gating)

    // ---- the per-frame angular-velocity smoother (before the state machine) ----
    {
        const rw::math::vpu::Vector3& lv3Angular =
            MomentSharedInfo_GetPlayerAngularVelocity(lSharedInfo);
        mSmoothedAngularVelocity = mSmoothedAngularVelocity
            + (lv3Angular - mSmoothedAngularVelocity) * KF_TUMBLE_SENSITIVITY;
    }

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        if (!MomentSharedInfo_IsPlayerCrashing(lSharedInfo))
        {
            mbFirstTryThisCrash    = true;
            mbUseLeftForThisCrash  = false;
            mbUseRightForThisCrash = false;
        }

        // ---- the takedown trigger ----
        bool lbTakedownCondition = false;
        if (MomentSharedInfo_WasTakedown(lSharedInfo) && mpParameters->mbTakedownMoment)
        {
            const s32 liVictim = MomentSharedInfo_GetTakedownVictimIndex(lSharedInfo);
            const AllVehicleData* lpAllVehicles = MomentSharedInfo_GetAllVehicleData(lSharedInfo);
            if (lpAllVehicles->GetUsedRaceCarsBitArray().IsBitSet(liVictim))   // the  tripwire
            {
                const rw::math::vpu::Vector3& lv3VictimVelocity =
                    lpAllVehicles->GetRaceCar(EActiveRaceCarIndex(liVictim))
                        .mRaceCarState.mLinearVelocity;   // VehicleInfo +816
                lbTakedownCondition =
                    rw::math::vpu::Dot(lv3VictimVelocity, lv3VictimVelocity) > KF_TUMBLE_START_THRESHOLD;
            }
        }

        // ---- the crash trigger ----
        bool lbCrashCondition = false;
        if (MomentSharedInfo_IsPlayerCrashing(lSharedInfo)
            && !MomentSharedInfo_IsCrashCameraBlocked(lSharedInfo))
        {
            const rw::math::vpu::Vector3& lv3Velocity =
                MomentSharedInfo_GetPlayerVelocity(lSharedInfo);
            lbCrashCondition =
                rw::math::vpu::Dot(lv3Velocity, lv3Velocity) > KF_TUMBLE_START_THRESHOLD
                && mpParameters->mbCrashMoment
                && !MomentSharedInfo_IsCrashReplayDisabled(lSharedInfo);
        }

        if (lbTakedownCondition || lbCrashCondition
            || MomentSharedInfo_GetForceFlag1320(lSharedInfo))
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
            }
            else
            {
                mbLookingAtTakedown = lbTakedownCondition && !lbCrashCondition;
                lpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                    mGyroCam, 0, this, 1);
                SetGyroCamParameters(lSharedInfo);
                if (mbLookingAtTakedown)
                {
                    mGyroCam.GetBehaviour()->AttachToRaceCar(
                        MomentSharedInfo_GetTakedownVictimIndex(lSharedInfo));
                }
                SetCanSwitchToMeNow(false);
                mfRunningTime = 0.0f;
                SetState(E_STATE_INVALID_FOUND_PREPARING);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
            }
        }
        else
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
        }
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mGyroCam.GetBehaviour()->CanSwitchToMeNow())
        {
            mfRunningTime       = 0.0f;
            mbFirstTryThisCrash = false;
            SetState(E_STATE_VALID);
            break;   // fall into the valid body this frame
        }
        if (mGyroCam.GetBehaviour()->HasFailed())
        {
            if (mbFirstTryThisCrash)
            {
                mbUseLeftForThisCrash  = false;
                mbUseRightForThisCrash = false;
            }
            mGyroCam.Release();
            SetState(E_STATE_INVALID_SEARCHING);
            return;
        }
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
        return;

    case E_STATE_VALID:
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   //  (non-gating)
        return;
    }

    // ---- the VALID body (runs the frame the rig becomes switchable and every
    // valid frame after) ----
    SetCamera(mGyroCam.GetProducedCamera());
    if (!mbLookingAtTakedown)
        GetNonConstCamera().mState.SetFlag(KU_STATE_FLAG_PLAYER_TUMBLE, true);

    // Re-evaluate the keep condition (the trigger tests sans the parameter gates).
    bool lbTakedownKeep = false;
    if (MomentSharedInfo_WasTakedown(lSharedInfo))
    {
        const s32 liVictim = MomentSharedInfo_GetTakedownVictimIndex(lSharedInfo);
        const AllVehicleData* lpAllVehicles = MomentSharedInfo_GetAllVehicleData(lSharedInfo);
        if (lpAllVehicles->GetUsedRaceCarsBitArray().IsBitSet(liVictim))   //  tripwire
        {
            const rw::math::vpu::Vector3& lv3VictimVelocity =
                lpAllVehicles->GetRaceCar(EActiveRaceCarIndex(liVictim))
                    .mRaceCarState.mLinearVelocity;   // VehicleInfo +816
            lbTakedownKeep =
                rw::math::vpu::Dot(lv3VictimVelocity, lv3VictimVelocity) > KF_TUMBLE_START_THRESHOLD;
        }
    }
    bool lbCrashKeep = false;
    if (MomentSharedInfo_IsPlayerCrashing(lSharedInfo))
    {
        const rw::math::vpu::Vector3& lv3Velocity =
            MomentSharedInfo_GetPlayerVelocity(lSharedInfo);
        lbCrashKeep =
            rw::math::vpu::Dot(lv3Velocity, lv3Velocity) > KF_TUMBLE_START_THRESHOLD
            && !MomentSharedInfo_IsCrashReplayDisabled(lSharedInfo);
    }

    // Publish the rig's own switch gates.
    if (!mGyroCam.GetBehaviour()->CanSwitchFromMeNow())
    {
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_NOT_RELEASABLE);
    }
    if (!mGyroCam.GetBehaviour()->CanSwitchToMeNow())
    {
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_NOT_SWITCHABLE);
    }

    // While the keep condition has lapsed, integrate the running time.
    const bool lbKeep = mbLookingAtTakedown ? lbTakedownKeep : lbCrashKeep;
    if (!lbKeep)
    {
        SetCanSwitchToMeNow(false);
        mfRunningTime += lfTimeStep;
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_LAPSED);
    }

    // FOLLOW/LEAD tumbles time out 0.5s past the lapse; they also keep the
    // motion blur up (0.25/1.0), the side subtypes run blur-free.
    const bool lbFollowOrLead =
        mpParameters->meSubType == Parameters::E_SUBTYPE_FOLLOW
        || mpParameters->meSubType == Parameters::E_SUBTYPE_LEAD;
    if (mfRunningTime > KF_TUMBLE_LAPSED_TIMEOUT && lbFollowOrLead)
    {
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_TIMED_OUT);
    }
    {
        Camera::MotionBlurData& lrBlur = GetNonConstCamera().GetEffects().mMotionBlurData;
        lrBlur.mbIsActive              = true;
        lrBlur.mbIsExpensiveMotionBlur = true;
        lrBlur.mfCarsBlurAmount        = lbFollowOrLead ? 0.25f : 0.0f;
        lrBlur.mfWorldBlurAmount       = 1.0f;
    }

    if (mGyroCam.GetBehaviour()->HasFailed())
    {
        mGyroCam.Release();
        SetState(E_STATE_INVALID_SEARCHING);
    }
}

}
