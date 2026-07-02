#include "GameSource/Director/MomentController/Moments/BrnMomentStaticCamImpact.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // Camera::BehaviourParameterBank

// BrnDirector::MomentStaticCamImpact -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnMomentStaticCamImpact.cpp).
//
// Bodied here (4 ledger functions):
//   Construct @0x8225F390   Update @0x82266AB0
//   Release   @0x8223AFF8   GetName @0x821F7660

namespace BrnDirector
{

namespace
{
    // Camera-state head bits the moment raises (per-bit roles not yet recovered --
    // see BrnCameraState.h; the sibling moments raise the same 18/19/23 family):
    //   18 (oris 4)      -- raised on Release
    //   19 (oris 8)      -- raised while the fixed cam is still preparing
    //   23 (oris 0x80)   -- raised while searching inhibited
    //   28 (oris 0x1000) -- raised every valid frame
    const u32 KU_CAMERA_HEAD_FLAG_RELEASED  = 18;
    const u32 KU_CAMERA_HEAD_FLAG_PREPARING = 19;
    const u32 KU_CAMERA_HEAD_FLAG_INHIBITED = 23;
    const u32 KU_CAMERA_HEAD_FLAG_VALID     = 28;
}

// @ 0x8225F390 -- the inlined base Moment::Construct (state/type/inhibit/camera init),
// then clear the fixed-cam handle fields and the parameter pointer.
void MomentStaticCamImpact::Construct()
{
    Moment::Construct();    // inlined on the X360 (meState/meType/mbIsInhibited/mCamera)
    mFixedCam.Clear();      // the X360 zeroes the five handle fields inline
    mpParameters = 0;
}

// @ 0x82266AB0 -- the per-frame static-cam state machine. The timestep/shared-info
// args are untouched by the X360 body (only the behaviour manager is read).
void MomentStaticCamImpact::Update(f32 lfTimeStep, void* lrBehaviourController,
                                   const void* lSharedInfo)
{
    (void)lfTimeStep;
    (void)lSharedInfo;

    Camera::BehaviourManager* lpBehaviourManager =
        static_cast<Camera::BehaviourManager*>(lrBehaviourController);

    switch (GetState())
    {
    case E_STATE_INVALID_SEARCHING:
        if (IsInhibited())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_INHIBITED);
        }
        else
        {
            lpBehaviourManager->NewBehaviour<Camera::BehaviourFixedCam>(
                mFixedCam, 0, this, 1);
            mFixedCam.GetBehaviour()->SetParameters(
                &lpBehaviourManager->GetBehaviourParameterBank().GetStaticCamImpactCamParams());
            SetState(E_STATE_VALID);
        }
        break;

    case E_STATE_INVALID_FOUND_PREPARING:
        if (mFixedCam.GetBehaviour()->HasFailed())
        {
            Release();   // virtual dispatch (the X360 calls through vtable slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
            break;
        }
        if (!mFixedCam.GetBehaviour()->CanSwitchToMeNow())
        {
            SetCanSwitchToMeNow(false);
            GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_PREPARING);
            break;
        }
        SetState(E_STATE_VALID);
        // deliberate fall-through into the valid-frame body (the X360 falls into it)

    case E_STATE_VALID:
        SetCanSwitchFromMeNow(false);
        GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_VALID);
        if (mFixedCam.GetBehaviour()->HasFailed())
        {
            Release();   // virtual dispatch (vtable slot 4)
            SetState(E_STATE_INVALID_SEARCHING);
        }
        else
        {
            SetCamera(mFixedCam.GetProducedCamera());
        }
        break;

    default:
        CGS_ASSERT(false, "unhandled case in switch");   // cpp:134
        break;
    }
}

// @ 0x8223AFF8 -- drop the fixed cam if held (the inlined BehaviourHandle::Release),
// clear the conditions/switch gates, raise head bit 18, and go back to SEARCHING
// (the hit-traffic sibling resets to INACTIVE here; this moment re-arms). Returns true.
bool MomentStaticCamImpact::Release()
{
    mFixedCam.Release();   // inlined on the X360 (guarded UnSetBehaviourUsedByHandle + field clear)
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_RELEASED);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

// @ 0x821F7660
const char* MomentStaticCamImpact::GetName() const
{
    return "MomentStaticCamImpact";
}

}
