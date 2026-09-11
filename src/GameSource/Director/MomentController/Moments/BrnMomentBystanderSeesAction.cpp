#include "GameSource/Director/MomentController/BrnMoment.h"   // MomentBystanderSeesAction (concretely homed there)

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"           // the bystander-cam moment blocks
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h" // BehaviourBystanderCam (SetParameters/SetTarget)

// BrnDirector::MomentBystanderSeesAction -- reconstructed from
// the console executable (home file BrnMomentBystanderSeesAction.cpp;
// member names verbatim from the declarations: mpParameters, mBystander
//
// Bodied here (5 ledger functions):
//   Construct   Update   SetParameters
//   Release   GetName
// (Prepare was bodied with the BrnMoment.h base TU.)

namespace BrnDirector
{

namespace
{
    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING      = 18;
    const u32 KU_HEAD_FLAG_ALLOCATED      = 19;
    const u32 KU_HEAD_FLAG_PREPARING      = 20;
    const u32 KU_HEAD_FLAG_INHIBITED      = 23;
    const u32 KU_HEAD_FLAG_NOT_SWITCHABLE = 24;   // (valid but the behaviour can't be switched to yet)

    const u32 KU_STATE_FLAG_KEEP_GATE = 1;   // mState current flag gating stay-valid

    // The motion blur the valid body forces on the mirrored camera each frame.
    const f32 KF_BYSTANDER_CARS_BLUR_AMOUNT  = 0.25f;
    const f32 KF_BYSTANDER_WORLD_BLUR_AMOUNT = 1.0f;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; console shared-info offsets in comments; role names
    // FLAG-inferred from the uses). ----
    bool MomentSharedInfo_IsPlayerCrashing(const void* lpSharedInfo);        // +1284 byte 249
    bool MomentSharedInfo_WasTakedown(const void* lpSharedInfo);             // +1284 byte 218 (the takedown-just-happened condition)
    bool MomentSharedInfo_IsCrashCameraBlocked(const void* lpSharedInfo);    // +1284 byte 449
    bool MomentSharedInfo_IsCrashReplayDisabled(const void* lpSharedInfo);   // +1284 byte 284
    s32  MomentSharedInfo_GetTakedownVictimIndex(const void* lpSharedInfo);  // +1284 word +224 (the takedown target car)
    s32  MomentSharedInfo_GetCrashVehicleIndex(const void* lpSharedInfo);    // +1304 word (the crashing car the bystander frames)
    bool MomentSharedInfo_GetForceFlag1320(const void* lpSharedInfo);        // +1320 byte (forces the crash condition)
}
using namespace detail;

// the inlined base Moment::Construct, the parameter-pointer
// reset, and the bystander-handle clear.
void MomentBystanderSeesAction::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera)
    mBystander.Clear();    // the console build zeroes the five handle fields inline
    mpParameters = 0;
}

// adopt the tuning record (no type assert in the console build).
void MomentBystanderSeesAction::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

// drop the bystander cam if held (the inlined handle Release),
// clear the conditions/switch gates, raise the searching head bit, reset the
// state. Returns true.
bool MomentBystanderSeesAction::Release()
{
    mBystander.Release();
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

const char* MomentBystanderSeesAction::GetName() const
{
    return "MomentBystanderSeesAction";
}

// the per-frame bystander state machine:
//   SEARCHING        two trigger conditions -- the CRASH one (crashing &&
//                    Parameters::mbCrashMoment && not blocked/disabled, or the
//                    force flag) framing the crashing car, and the TAKEDOWN one
//                    (the takedown byte && Parameters::mbTakedownMoment) framing
//                    the victim. Either allocates a bystander cam with the
//                    close/far parameter block (Parameters::mbCloseCamera) and
//                    its target, then enters FOUND_PREPARING.
//   FOUND_PREPARING  failed -> Release (the virtual); not yet switchable
//                    hold; else VALID (and run the valid tail this frame).
//   VALID            (the shared switch tail) mirror the produced camera, force
//                    the bystander motion blur, and drop the moment when the
//                    conditions/keep gate lapse.
void MomentBystanderSeesAction::Update(f32 /*lfTimeStep*/, void* lrBehaviourController,
                                       const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   //  (non-gating)

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
    {
        // The close/far parameter block (manager bank +0xEEC / +0x1024).
        const Camera::BehaviourBystanderCam::Parameters& lrCamParams =
            mpParameters->mbCloseCamera
                ? lpBehaviourManager->GetBehaviourParameterBank().GetBystanderCamCloseMomentParams()
                : lpBehaviourManager->GetBehaviourParameterBank().GetBystanderCamMomentParams();

        const bool lbCrashCondition =
            (MomentSharedInfo_IsPlayerCrashing(lSharedInfo)
             && mpParameters->mbCrashMoment
             && !MomentSharedInfo_IsCrashCameraBlocked(lSharedInfo)
             && !MomentSharedInfo_IsCrashReplayDisabled(lSharedInfo))
            || MomentSharedInfo_GetForceFlag1320(lSharedInfo);

        const bool lbTakedownCondition =
            MomentSharedInfo_WasTakedown(lSharedInfo) && mpParameters->mbTakedownMoment;

        if (lbCrashCondition)
        {
            if (!IsInhibited())
            {
                lpBehaviourManager->NewBehaviour<Camera::BehaviourBystanderCam>(
                    mBystander, 0, this, 1);
                mBystander.GetBehaviour()->SetParameters(&lrCamParams);
                mBystander.GetBehaviour()->SetTarget(
                    MomentSharedInfo_GetCrashVehicleIndex(lSharedInfo));
                SetCanSwitchToMeNow(false);
                SetState(E_STATE_INVALID_FOUND_PREPARING);
                GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
                return;
            }
        }
        else if (!lbTakedownCondition)
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_SEARCHING);
            return;
        }
        else if (!IsInhibited())
        {
            lpBehaviourManager->NewBehaviour<Camera::BehaviourBystanderCam>(
                mBystander, 0, this, 1);
            mBystander.GetBehaviour()->SetParameters(&lrCamParams);
            mBystander.GetBehaviour()->SetTarget(
                MomentSharedInfo_GetTakedownVictimIndex(lSharedInfo));
            SetCanSwitchToMeNow(false);
            SetState(E_STATE_INVALID_FOUND_PREPARING);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_ALLOCATED);
            return;
        }

        // Inhibited (either condition path).
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_INHIBITED);
        return;
    }

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mBystander.GetBehaviour()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            return;
        }
        if (!mBystander.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_PREPARING);
            return;
        }
        SetState(E_STATE_VALID);
        break;   // fall into the shared valid tail

    case E_STATE_VALID:
        break;   // the shared valid tail

    default:
        CGS_ASSERT(false, "unhandled case in switch");   //  (non-gating)
        return;
    }

    // ---- the shared VALID tail (runs the frame the behaviour becomes switchable
    // and every valid frame after) ----
    SetCamera(mBystander.GetProducedCamera());
    {
        Camera::MotionBlurData& lrBlur =
            GetNonConstCamera().GetEffects().mMotionBlurData;
        lrBlur.mfCarsBlurAmount        = KF_BYSTANDER_CARS_BLUR_AMOUNT;
        lrBlur.mfWorldBlurAmount       = KF_BYSTANDER_WORLD_BLUR_AMOUNT;
        lrBlur.mbIsActive              = true;
        lrBlur.mbIsExpensiveMotionBlur = true;
    }
    if (!mBystander.GetBehaviour()->CanSwitchToMeNow())
    {
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_NOT_SWITCHABLE);
    }

    if ((!MomentSharedInfo_IsPlayerCrashing(lSharedInfo)
         && !MomentSharedInfo_WasTakedown(lSharedInfo))
        || MomentSharedInfo_IsCrashReplayDisabled(lSharedInfo)
        || !mBystander.GetProducedCamera().mState.IsFlagSet(KU_STATE_FLAG_KEEP_GATE))
    {
        Release();   // the live-vtable call (slot 4)
    }
}

// MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor
//
// ⭐ ADDED 2026-08-29 (crash-camera wave). ArbStateCrashing::Update calls this with 0.5 once a
// bystander shot has held a crash for longer than kfMomentTime, pulling the framing in so the
// car stays readable in a long crash.
// The console body, in order:
//   read mBystander's allocated flag at +0x184  ; assert "IsAllocated "
//   resolve the behaviour through the handle words at +0x188 / +0x18C (GetBehaviour)
//   read the behaviour's +0x358 float and compare it with the new value;
//     equal -> return, so it ONLY writes when the value actually changes
//   store the new value to behaviour +0x358      ; mfPerceivedDistanceModificationFactor
//   set the behaviour's +0xCF flag byte          ; the "re-solve the vantage" latch
// The inequality guard is the whole point: raising the latch every frame would make the camera
// re-solve its vantage continuously.
void MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor(f32 lfFactor)
{
    CGS_ASSERT(mBystander.IsAllocated(), "IsAllocated()");

    Camera::BehaviourBystanderCam* lpBystander = mBystander.GetBehaviour();

    if (lfFactor != lpBystander->GetPerceivedDistanceModificationFactor())
    {
        lpBystander->SetPerceivedDistanceModificationFactor(lfFactor);
    }
}

}
