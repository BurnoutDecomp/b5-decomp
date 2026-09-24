#include "GameSource/Director/MomentController/Moments/BrnMomentHitTraffic.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // Camera::BehaviourParameterBank

// BrnDirector::MomentHitTraffic -- reconstructed from the console executable
// (home file BrnMomentHitTraffic.cpp; call structure cross-checked against
// the declarations' own BrnDirectorUnity.cpp body hints for Update).
//
// Bodied here (4 ledger functions):
//   Construct   Update
//   Release   GetName

namespace BrnDirector
{

namespace
{
    // The moment holds its gyro cam for at most this long past the condition
    // (console read-only data, 1.0f; the compare is `>` so exactly 1s still holds).
    const f32 KF_HIT_TRAFFIC_MAX_RUNNING_TIME = 1.0f;

    // Camera-state head bits the moment raises (same head set as the fail-safe's bit
    // 18; the head set's per-bit roles are not yet recovered -- see BrnCameraState.h):
    //   18    -- raised while searching with the condition absent / on release
    //   19    -- raised when the gyro cam is freshly allocated
    //   23 -- raised while searching inhibited
    const u32 KU_CAMERA_HEAD_FLAG_SEARCHING     = 18;
    const u32 KU_CAMERA_HEAD_FLAG_GYRO_ALLOCATED = 19;
    const u32 KU_CAMERA_HEAD_FLAG_INHIBITED      = 23;
}

namespace detail
{
    // The hit-traffic condition byte Update polls at MomentSharedInfo +0x44A (the console
    // reads that byte directly). FLAG: the MomentSharedInfo record is un-homed
    // (the Moment base type-erases it to const void*); this named helper is
    // DECLARATION-ONLY per the Behaviour.cpp foreign-reach precedent -- replace with
    // the real accessor when the MomentSharedInfo home lands. The role (the
    // "player is hitting traffic" condition: gates allocation while searching and
    // freezes the running-time integrator while valid) is inferred from both uses.
    bool MomentSharedInfo_IsHitTrafficConditionActive(const void* lpSharedInfo);
}
using namespace detail;

// the inlined base Moment::Construct (state/type/inhibit/camera init),
// then clear the heli-cam handle fields and the parameter pointer. mfRunningTime is
// NOT touched here (the console build leaves it; it is seeded on allocation in Update).
void MomentHitTraffic::Construct()
{
    Moment::Construct();       // inlined in the console build (meState/meType/mbIsInhibited/mCamera)
    mHeliCamHandle.Clear();    // the console build zeroes the five handle fields inline
    mpParameters = 0;
}

// The per-frame hit-traffic state machine (declared locals
// lrParameterBank; call set: SetCantSwitchToMeNow/SetBit/GetCamera/SetCamera/Release/
// SetState/SetConditionsNotMet/Get/GetBehaviourParameterBank/SetParameters).
void MomentHitTraffic::Update(f32 lfTimeStep, void* lrBehaviourController,
                              const void* lSharedInfo)
{
    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
        if (MomentSharedInfo_IsHitTrafficConditionActive(lSharedInfo))
        {
            if (IsInhibited())
            {
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_INHIBITED);
            }
            else
            {
                mfRunningTime = 0.0f;
                lpBehaviourManager->NewBehaviour<Camera::BehaviourGyroCam>(
                    mHeliCamHandle, 0, this, 1);
                const Camera::BehaviourParameterBank& lrParameterBank =
                    lpBehaviourManager->GetBehaviourParameterBank();
                mHeliCamHandle.GetBehaviour()->SetParameters(
                    &lrParameterBank.GetGyroCamMomentParams());
                SetCanSwitchToMeNow(false);
                GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_GYRO_ALLOCATED);
                SetState(E_STATE_VALID);
            }
        }
        else
        {
            SetConditionsNotMet();
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_SEARCHING);
        }
        break;

    case E_STATE_VALID:
        SetCamera(mHeliCamHandle.GetProducedCamera());
        if (!MomentSharedInfo_IsHitTrafficConditionActive(lSharedInfo))
            mfRunningTime += lfTimeStep;
        if (mfRunningTime > KF_HIT_TRAFFIC_MAX_RUNNING_TIME
            || mHeliCamHandle.GetBehaviour()->HasFailed())
        {
            mHeliCamHandle.Release();
            SetState(E_STATE_INVALID_SEARCHING);
        }
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");
        break;
    }
}

// drop the gyro cam if held (the inlined BehaviourHandle::Release),
// clear the conditions/switch gates, raise the searching head bit, reset the state.
// Returns true.
bool MomentHitTraffic::Release()
{
    mHeliCamHandle.Release();   // inlined in the console build (guarded UnSetBehaviourUsedByHandle + field clear)
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_INACTIVE);
    return true;
}

const char* MomentHitTraffic::GetName() const
{
    return "MomentHitTraffic";
}

}

// ---- [FX-DIRECTOR 2026-09-24] the vtable one-liners the moment factory needs --------------------
// MomentController::NewMoment's AllocateVoid<MomentHitTraffic> placement-constructs the moment, which emits its
// vftable, so every slot needs a body. Read off the console vftable off_8200740C (AllocateVoid<MomentHitTraffic>
// @0x8222E610 stores it at +0) and the ICF-folded slot bodies it points at:
//   slot 1 Prepare          0x821F7560  meState = E_STATE_INVALID_SEARCHING, return true (the shared
//                                       body the export names MomentBystanderSeesAction::Prepare)
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
//   slot 3 SetParameters    0x821F76A8  `stw r4, 0x194(r3)` -- mpParameters
//   slot 7 GetInstanceType  0x82C296C8  `li r3, 1 ; blr` -- E_MOMENT_HIT_TRAFFIC
namespace BrnDirector
{
bool MomentHitTraffic::Prepare(void* /*lrBehaviourController*/)
{
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

void MomentHitTraffic::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}

void MomentHitTraffic::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);   // stw r4, 0x194(r3)
}

Moment::EType MomentHitTraffic::GetInstanceType()
{
    return E_MOMENT_HIT_TRAFFIC;   // li r3, 1
}
}
