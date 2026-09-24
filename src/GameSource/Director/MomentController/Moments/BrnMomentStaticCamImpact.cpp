#include "GameSource/Director/MomentController/Moments/BrnMomentStaticCamImpact.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"      // Camera::BehaviourParameterBank

// BrnDirector::MomentStaticCamImpact -- reconstructed from the console executable
// (home file BrnMomentStaticCamImpact.cpp).
//
// Bodied here (4 ledger functions):
//   Construct   Update
//   Release   GetName

namespace BrnDirector
{

namespace
{
    // Camera-state head bits the moment raises (per-bit roles not yet recovered --
    // see BrnCameraState.h; the sibling moments raise the same 18/19/23 family):
    //   18      -- raised on Release
    //   19      -- raised while the fixed cam is still preparing
    //   23   -- raised while searching inhibited
    //   28 -- raised every valid frame
    const u32 KU_CAMERA_HEAD_FLAG_RELEASED  = 18;
    const u32 KU_CAMERA_HEAD_FLAG_PREPARING = 19;
    const u32 KU_CAMERA_HEAD_FLAG_INHIBITED = 23;
    const u32 KU_CAMERA_HEAD_FLAG_VALID     = 28;
}

// the inlined base Moment::Construct (state/type/inhibit/camera init),
// then clear the fixed-cam handle fields and the parameter pointer.
void MomentStaticCamImpact::Construct()
{
    Moment::Construct();    // inlined in the console build (meState/meType/mbIsInhibited/mCamera)
    mFixedCam.Clear();      // the console build zeroes the five handle fields inline
    mpParameters = 0;
}

// the per-frame static-cam state machine. The timestep/shared-info
// args are untouched by the console build body (only the behaviour manager is read).
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
            Release();   // virtual dispatch (the console build calls through vtable slot 4)
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
        // deliberate fall-through into the valid-frame body (the console build falls into it)

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
        CGS_ASSERT(false, "unhandled case in switch");
        break;
    }
}

// drop the fixed cam if held (the inlined BehaviourHandle::Release),
// clear the conditions/switch gates, raise head bit 18, and go back to SEARCHING
// (the hit-traffic sibling resets to INACTIVE here; this moment re-arms). Returns true.
bool MomentStaticCamImpact::Release()
{
    mFixedCam.Release();   // inlined in the console build (guarded UnSetBehaviourUsedByHandle + field clear)
    SetConditionsNotMet();
    SetCanSwitchToMeNow(false);
    GetNonConstCamera().mState.SetHeadFlag(KU_CAMERA_HEAD_FLAG_RELEASED);
    SetState(E_STATE_INVALID_SEARCHING);
    return true;
}

const char* MomentStaticCamImpact::GetName() const
{
    return "MomentStaticCamImpact";
}

}

// ---- [FX-DIRECTOR 2026-09-24] the vtable one-liners the moment factory needs --------------------
// MomentController::NewMoment's AllocateVoid<MomentStaticCamImpact> placement-constructs the moment, which emits its
// vftable, so every slot needs a body. Read off the console vftable off_820074CC (AllocateVoid<MomentStaticCamImpact>
// @0x8222EB80 stores it at +0) and the ICF-folded slot bodies it points at:
//   slot 1 Prepare          0x821F7560  meState = E_STATE_INVALID_SEARCHING, return true (the shared
//                                       body the export names MomentBystanderSeesAction::Prepare)
//   slot 5 Destruct         0x8284CB38  `blr` (the one empty body all twelve moments share)
//   slot 3 SetParameters    0x821F76A8  `stw r4, 0x194(r3)` -- mpParameters
//   slot 7 GetInstanceType  0x821F7658  `li r3, 9 ; blr` -- E_MOMENT_STATIC_CAM_IMPACT
namespace BrnDirector
{
bool MomentStaticCamImpact::Prepare(void* /*lrBehaviourController*/)
{
    SetState(E_STATE_INVALID_SEARCHING);   // li r10, 1 ; stw r10, 0x174(r3)
    return true;                           // li r3, 1
}

void MomentStaticCamImpact::Destruct()
{
    // 0x8284CB38 is a lone `blr`: nothing to tear down.
}

void MomentStaticCamImpact::SetParameters(const Moment::Parameters* lpParameters)
{
    mpParameters = static_cast<const Parameters*>(lpParameters);   // stw r4, 0x194(r3)
}

Moment::EType MomentStaticCamImpact::GetInstanceType()
{
    return E_MOMENT_STATIC_CAM_IMPACT;   // li r3, 9
}
}
