#include "GameSource/Director/MomentController/Moments/BrnMomentHitTraffic.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // Camera::BehaviourParameterBank

// BrnDirector::MomentHitTraffic -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentHitTraffic.cpp; call structure cross-checked against
// the PS3 DecFIGS BrnDirectorUnity.cpp body hints for Update).
//
// Bodied here (4 ledger functions):
//   Construct @0x8225ECB0   Update @0x82271D90
//   Release   @0x8223A918   GetName @0x821F7578

namespace BrnDirector
{

namespace
{
    // The moment holds its gyro cam for at most this long past the condition
    // (X360 rodata flt_82001C98 == 1.0f; the compare is `>` so exactly 1s still holds).
    const f32 KF_HIT_TRAFFIC_MAX_RUNNING_TIME = 1.0f;

    // Camera-state head bits the moment raises (same head set as the fail-safe's bit
    // 18; the head set's per-bit roles are not yet recovered -- see BrnCameraState.h):
    //   18 (oris 4)    -- raised while searching with the condition absent / on release
    //   19 (oris 8)    -- raised when the gyro cam is freshly allocated
    //   23 (oris 0x80) -- raised while searching inhibited
    const u32 KU_CAMERA_HEAD_FLAG_SEARCHING     = 18;
    const u32 KU_CAMERA_HEAD_FLAG_GYRO_ALLOCATED = 19;
    const u32 KU_CAMERA_HEAD_FLAG_INHIBITED      = 23;
}

namespace detail
{
    // The hit-traffic condition byte Update polls at MomentSharedInfo +0x44A (X360
    // `lbz r11,0x44A(info)`). FLAG: the MomentSharedInfo record is un-homed
    // (the Moment base type-erases it to const void*); this named helper is
    // DECLARATION-ONLY per the Behaviour.cpp foreign-reach precedent -- replace with
    // the real accessor when the MomentSharedInfo home lands. The role (the
    // "player is hitting traffic" condition: gates allocation while searching and
    // freezes the running-time integrator while valid) is inferred from both uses.
    bool MomentSharedInfo_IsHitTrafficConditionActive(const void* lpSharedInfo);
}
using namespace detail;

// @ 0x8225ECB0 -- the inlined base Moment::Construct (state/type/inhibit/camera init),
// then clear the heli-cam handle fields and the parameter pointer. mfRunningTime is
// NOT touched here (the X360 leaves it; it is seeded on allocation in Update).
void MomentHitTraffic::Construct()
{
    Moment::Construct();       // inlined on the X360 (meState/meType/mbIsInhibited/mCamera)
    mHeliCamHandle.Clear();    // the X360 zeroes the five handle fields inline
    mpParameters = 0;
}

// @ 0x82271D90 -- the per-frame hit-traffic state machine (PS3 locals cpp:86
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
                const Camera::BehaviourParameterBank& lrParameterBank =           // :86
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
        CGS_ASSERT(false, "unhandled case in switch");   // cpp:125
        break;
    }
}

// @ 0x8223A918 -- drop the gyro cam if held (the inlined BehaviourHandle::Release),
// clear the conditions/switch gates, raise the searching head bit, reset the state.
// Returns true.
bool MomentHitTraffic::Release()
{
    mHeliCamHandle.Release();   // inlined on the X360 (guarded UnSetBehaviourUsedByHandle + field clear)
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_SEARCHING);
    SetState(E_STATE_INVALID_INACTIVE);
    return true;
}

// @ 0x821F7578
const char* MomentHitTraffic::GetName() const
{
    return "MomentHitTraffic";
}

}
