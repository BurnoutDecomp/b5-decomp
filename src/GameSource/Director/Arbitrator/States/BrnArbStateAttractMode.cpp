#include "GameSource/Director/Arbitrator/States/BrnArbStateAttractMode.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (unhandled-state assert)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::GetState/SetCurrentState
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                 // Camera::BehaviourManager (complete)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"   // Camera::BehaviourRoadRunner (slice; GetProducedCamera)
#include "GameSource/Director/Camera/Camera.h"                              // Camera::Camera (operator= / Construct)

// ============================================================================
// BrnDirector::ArbStateAttractMode -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x8225B1C8
//   GetName    @0x821F6700
//   Update     @0x822361F0
//   Release    @0x82236320
//
// The front-end "attract mode" director state: it drives a single road-runner fly-by camera
// behaviour, copies the camera it produces each frame into the state's own camera, and -- once
// the roaming state accepts a Prepare -- switches the arbitrator's current state to roaming and
// releases itself. All member access is BY NAME; the camera copy goes through the road-runner
// handle's named GetProducedCamera() accessor (mirrors ArbStateRoaming / ArbStateCrashNav), and
// the roaming hand-off goes through the state container's named GetState()/SetCurrentState().
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // BehaviourHandle::GetProducedCamera -- the camera the live road-runner behaviour produced
    // this frame (the X360 reaches it through the handle helper sub_821FDC58). Defined out-of-line
    // here, where BehaviourRoadRunner is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    const Camera::Camera& ArbStateAttractMode::BehaviourHandle<TBehaviour>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return mpBehaviour->GetProducedCamera();
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225B1C8 -- build the camera and zero the state machine + the road-runner
    // behaviour handle.
    // ------------------------------------------------------------------------
    void ArbStateAttractMode::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x171 / +0x170

        meState = E_STATE_INACTIVE;        // +0x194 = 0

        // The road-runner handle starts unallocated (+0x180 block zeroed).
        mRoadRunnerCam = BehaviourHandle<Camera::BehaviourRoadRunner>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6700
    // ------------------------------------------------------------------------
    const char* ArbStateAttractMode::GetName() const
    {
        return "ArbStateAttractMode";
    }

    // ------------------------------------------------------------------------
    // Update @0x822361F0 -- the attract-mode per-frame tick. Dispatches on meState.
    //   INACTIVE            : nothing to do.
    //   PREPARING           : run Prepare() (X360 virtual call (*(*this+4))(this, info)); on
    //                         success enter ACTIVE and run the ACTIVE body this same frame.
    //   ACTIVE              : copy the road-runner behaviour's produced camera into mCamera.
    //   CHANGING_TO_ROAMING : copy the produced camera, then try to Prepare() the roaming state;
    //                         on success make roaming the arbitrator's current state and Release()
    //                         this state (X360 tail virtual call, vtable slot +0xC).
    // ------------------------------------------------------------------------
    void ArbStateAttractMode::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            // X360: (*(*this+4))(this, info) -- the virtual Prepare call.
            if (Prepare(lrSharedInfo))
            {
                meState = E_STATE_ACTIVE;   // +0x194 = 2
                goto active_body;
            }
            break;

        case E_STATE_ACTIVE:
        active_body:
            GetNonConstCamera() = mRoadRunnerCam.GetProducedCamera();
            break;

        case E_STATE_CHANGING_TO_ROAMING:
        {
            GetNonConstCamera() = mRoadRunnerCam.GetProducedCamera();

            // X360: mpStateContainer + 0x1890 -> the embedded roaming state sub-object; virtual
            // call at vtable slot +4 (Prepare).
            ArbitratorState* lpRoamingState =
                lrSharedInfo.mpStateContainer->GetState(ArbitratorStateContainer::E_STATE_ROAMING);

            if (lpRoamingState->Prepare(lrSharedInfo))
            {
                // X360: lwz +0x35A4(mpStateContainer) -> stw +0x35CC(mpStateContainer) -- makes
                // roaming the arbitrator's current state.
                lrSharedInfo.mpStateContainer->SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
                // X360 tail: (*(*this+0xC))(this, info) -- the virtual Release call.
                Release(lrSharedInfo);
            }
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82236320 -- leave attract mode: drop to INACTIVE, release the road-runner
    // behaviour handle back to the manager if allocated, and assert no behaviours remain
    // allocated by this state.
    // ------------------------------------------------------------------------
    bool ArbStateAttractMode::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;        // +0x194 = 0

        if (mRoadRunnerCam.mbAllocated)    // +0x180 block
        {
            mRoadRunnerCam.mpManager->UnSetBehaviourUsedByHandle(mRoadRunnerCam.muAllocationKey);
            mRoadRunnerCam.muHelperIndex = 0;
            mRoadRunnerCam.mpManager     = 0;
            mRoadRunnerCam.mpBehaviour   = 0;
            mRoadRunnerCam.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
