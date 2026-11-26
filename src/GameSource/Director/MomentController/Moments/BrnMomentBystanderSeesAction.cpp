#include "GameSource/Director/MomentController/BrnMoment.h"   // MomentBystanderSeesAction (concretely homed there)
#include "GameSource/Director/MomentController/BrnMomentSharedInfo.h"
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Director/MomentController/Moments/BrnMomentBystanderSeesAction.cpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"           // the bystander-cam moment blocks
#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"    // BehaviourBystanderCam (SetParameters/SetTarget)

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

    // The motion blur the valid body forces on the mirrored camera each frame.
    const f32 KF_BYSTANDER_CARS_BLUR_AMOUNT  = 0.25f;
    const f32 KF_BYSTANDER_WORLD_BLUR_AMOUNT = 1.0f;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; console shared-info offsets in comments; role names
    // FLAG-inferred from the uses). ----
}
using namespace detail;

// the inlined base Moment::Construct, the parameter-pointer
// reset, and the bystander-handle clear.
void MomentBystanderSeesAction::Construct()
{
    Moment::Construct();   // inlined in the console build (state/type/inhibit/camera)
    mBystander.Construct();    // the console build zeroes the five handle fields inline
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
void MomentBystanderSeesAction::Update(f32 /*lfTimeStep*/, Camera::BehaviourManager& lrBehaviourController,
                                       const MomentSharedInfo& lrSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        &lrBehaviourController;

    CGS_ASSERT_AT(mpParameters != 0, "mpParameters != NULL", CGS_ARTIST_FILE, 87);   //  (non-gating)

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
            (lrSharedInfo.mpGameState->mbCrashActive
             && mpParameters->mbCrashMoment
             && !lrSharedInfo.mpGameState->mbRoadRageTotalled
             && !lrSharedInfo.mpGameState->mbGoToCrashModeAfterIntro)
            || lrSharedInfo.mbForceCollisionPolicysToStart;

        const bool lbTakedownCondition =
            lrSharedInfo.mpGameState->mbTakedownActive && mpParameters->mbTakedownMoment;

        if (lbCrashCondition)
        {
            if (!IsInhibited())
            {
                lpBehaviourManager->NewBehaviour<Camera::BehaviourBystanderCam>(
                    mBystander, 0, this, 1);
                mBystander.Get()->SetParameters(&lrCamParams);
                mBystander.Get()->SetTarget(
                    static_cast<EActiveRaceCarIndex>(lrSharedInfo.mePlayerActiveRaceCarIndex));
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
            mBystander.Get()->SetParameters(&lrCamParams);
            mBystander.Get()->SetTarget(
                static_cast<EActiveRaceCarIndex>(lrSharedInfo.mpGameState->meTakedownVictimID));
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
        if (mBystander.Get()->HasFailed())
        {
            Release();   // the live-vtable call (slot 4)
            return;
        }
        if (!mBystander.Get()->CanSwitchToMeNow())
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
        CGS_FAIL_AT("unhandled case in switch", CGS_ARTIST_FILE, 196);   //  (non-gating)
        return;
    }

    // ---- the shared VALID tail (runs the frame the behaviour becomes switchable
    // and every valid frame after) ----
    SetCamera(mBystander.GetCamera());
    {
        Camera::MotionBlurData& lrBlur =
            GetNonConstCamera().GetEffects().mMotionBlurData;
        lrBlur.mfCarsBlurAmount        = KF_BYSTANDER_CARS_BLUR_AMOUNT;
        lrBlur.mfWorldBlurAmount       = KF_BYSTANDER_WORLD_BLUR_AMOUNT;
        lrBlur.mbIsActive              = true;
        lrBlur.mbIsExpensiveMotionBlur = true;
    }
    if (!mBystander.Get()->CanSwitchToMeNow())
    {
        SetCanSwitchToMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_HEAD_FLAG_NOT_SWITCHABLE);
    }

    if ((!lrSharedInfo.mpGameState->mbCrashActive
         && !lrSharedInfo.mpGameState->mbTakedownActive)
        || lrSharedInfo.mpGameState->mbGoToCrashModeAfterIntro
        || !mBystander.GetCamera().mState.IsFlagSet(Camera::CameraState::E_FLAG_VALID))
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
//   then the INLINED BehaviourBystanderCam::SetPerceivedDistanceModificationFactor (h:268):
//     read the behaviour's +0x358 float and compare it with the new value;
//       equal -> return, so it ONLY writes when the value actually changes
//     store the new value to behaviour +0x358      ; mfPerceivedDistanceModificationFactor
//     `stb 1` to the behaviour's +0xCF             ; mLooker (+0xB0).mbForceZoomTargetUpdate
// The inequality guard is the behaviour's own: raising the looker's latch every frame would make
// the zoom re-snap its FOV band continuously.
void MomentBystanderSeesAction::SetPerceivedDistanceModificationFactor(f32 lfFactor)
{
    CGS_ASSERT_AT(mBystander.IsAllocated(), "IsAllocated()", "..\\..\\..\\GameSource\\Director/Camera/BrnBehaviourManager.h", 589);

    mBystander.Get()->SetPerceivedDistanceModificationFactor(lfFactor);
}

}

#undef CGS_ARTIST_FILE
