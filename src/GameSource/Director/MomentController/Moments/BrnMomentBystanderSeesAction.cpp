#include "GameSource/Director/MomentController/BrnMoment.h"   // MomentBystanderSeesAction (concretely homed there)

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"           // the bystander-cam moment blocks
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h" // BehaviourBystanderCam (SetParameters/SetTarget)

// BrnDirector::MomentBystanderSeesAction -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file BrnMomentBystanderSeesAction.cpp;
// member names verbatim from the DecFIGS DWARF: mpParameters h:87, mBystander
// h:89).
//
// Bodied here (5 ledger functions):
//   Construct @0x8225F118   Update @0x82266730   SetParameters @0x821F7670
//   Release   @0x8223AAF8   GetName @0x821F7608
// (Prepare @0x821F7560 was bodied with the BrnMoment.h base TU.)

namespace BrnDirector
{

namespace
{
    // Camera-state head bits (the moment family's shared vocabulary):
    const u32 KU_HEAD_FLAG_SEARCHING      = 18;   // oris 4
    const u32 KU_HEAD_FLAG_ALLOCATED      = 19;   // oris 8
    const u32 KU_HEAD_FLAG_PREPARING      = 20;   // oris 0x10
    const u32 KU_HEAD_FLAG_INHIBITED      = 23;   // oris 0x80
    const u32 KU_HEAD_FLAG_NOT_SWITCHABLE = 24;   // oris 0x100 (valid but the behaviour can't be switched to yet)

    const u32 KU_STATE_FLAG_KEEP_GATE = 1;   // mState current flag gating stay-valid

    // The motion blur the valid body forces on the mirrored camera each frame.
    const f32 KF_BYSTANDER_CARS_BLUR_AMOUNT  = 0.25f;
    const f32 KF_BYSTANDER_WORLD_BLUR_AMOUNT = 1.0f;
}

namespace detail
{
    // ---- MomentSharedInfo reaches (un-homed record; the family precedent's
    // decl-only helpers; X360 shared-info offsets in comments; role names
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

// @ 0x8225F118 -- the inlined base Moment::Construct, the parameter-pointer
// reset, and the bystander-handle clear.
void MomentBystanderSeesAction::Construct()
{
    Moment::Construct();   // inlined on the X360 (state/type/inhibit/camera)
    mBystander.Clear();    // the X360 zeroes the five handle fields inline
    mpParameters = 0;
}

// @ 0x821F7670 -- adopt the tuning record (no type assert on the X360).
void MomentBystanderSeesAction::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

// @ 0x8223AAF8 -- drop the bystander cam if held (the inlined handle Release),
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

// @ 0x821F7608.
const char* MomentBystanderSeesAction::GetName() const
{
    return "MomentBystanderSeesAction";
}

// @ 0x82266730 -- the per-frame bystander state machine:
//   SEARCHING        two trigger conditions -- the CRASH one (crashing &&
//                    Parameters::mbCrashMoment && not blocked/disabled, or the
//                    force flag) framing the crashing car, and the TAKEDOWN one
//                    (the takedown byte && Parameters::mbTakedownMoment) framing
//                    the victim. Either allocates a bystander cam with the
//                    close/far parameter block (Parameters::mbCloseCamera) and
//                    its target, then enters FOUND_PREPARING.
//   FOUND_PREPARING  failed -> Release (the virtual); not yet switchable ->
//                    hold; else VALID (and run the valid tail this frame).
//   VALID            (the shared switch tail) mirror the produced camera, force
//                    the bystander motion blur, and drop the moment when the
//                    conditions/keep gate lapse.
void MomentBystanderSeesAction::Update(f32 /*lfTimeStep*/, void* lrBehaviourController,
                                       const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");   // :87 (non-gating)

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
        CGS_ASSERT(false, "unhandled case in switch");   // :196 (non-gating)
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

}
