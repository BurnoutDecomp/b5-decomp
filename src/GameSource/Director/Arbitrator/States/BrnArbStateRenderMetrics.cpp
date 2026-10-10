// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateRenderMetrics.cpp
//
// BrnDirector::ArbStateRenderMetrics -- the render-metrics arbitrator state: Construct, Prepare,
// Update, Release and GetName.
// ============================================================================

#include "GameSource/Director/Arbitrator/States/BrnArbStateRenderMetrics.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CGS_ASSERT
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"  // GetState / SetCurrentState
#include "GameSource/Director/Camera/Camera.h"                                   // Camera::Camera (Construct / operator=)
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                      // NewBehaviour<> / CheckNoBehavioursAreAllocatedByState
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h"     // Camera::BehaviourRenderMetrics

namespace BrnDirector
{
    // ------------------------------------------------------------------------
    // Construct -- build the base camera, clear its two flags, reset the state machine and leave
    // the behaviour handle empty.
    // ------------------------------------------------------------------------
    void ArbStateRenderMetrics::Construct()
    {
        GetNonConstCamera().Construct();
        ResetBaseCameraFlags();
        meState = E_STATE_INACTIVE;
        mCam.Clear();
    }

    // ------------------------------------------------------------------------
    // Prepare -- already up (ACTIVE / CHANGING_TO_ROAMING) is ready. Otherwise go to PREPARING;
    // with the probe behaviour already allocated that is ready too, else allocate it (owned by
    // this state, reference limit 1) and report not ready yet.
    // ------------------------------------------------------------------------
    bool ArbStateRenderMetrics::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
        {
            return true;
        }

        meState = E_STATE_PREPARING;

        if (!mCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourRenderMetrics>(mCam, this, 0, 1);
            return false;
        }

        return true;
    }

    // ------------------------------------------------------------------------
    // Update -- the state machine.
    //   INACTIVE            nothing.
    //   PREPARING           Prepare again; once ready go ACTIVE and run ACTIVE this frame.
    //   ACTIVE              copy the probe behaviour's camera into this state's camera.
    //   CHANGING_TO_ROAMING copy the camera, then ask the roaming state to Prepare; once it
    //                       accepts, make roaming the container's current state and Release.
    // ------------------------------------------------------------------------
    void ArbStateRenderMetrics::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            if (!Prepare(lrSharedInfo))
            {
                break;
            }
            meState = E_STATE_ACTIVE;
            // fall through
        case E_STATE_ACTIVE:
            GetNonConstCamera() = mCam.GetProducedCamera();
            break;

        case E_STATE_CHANGING_TO_ROAMING:
        {
            GetNonConstCamera() = mCam.GetProducedCamera();

            ArbitratorState* lpRoamingState =
                lrSharedInfo.mpStateContainer->GetState(ArbitratorStateContainer::E_STATE_ROAMING);
            if (lpRoamingState->Prepare(lrSharedInfo))
            {
                lrSharedInfo.mpStateContainer->SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
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
    // Release -- back to INACTIVE, give the probe behaviour back and check the manager holds
    // nothing on this state's behalf.
    // ------------------------------------------------------------------------
    bool ArbStateRenderMetrics::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;
        mCam.Release();
        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }

    const char* ArbStateRenderMetrics::GetName() const
    {
        return "ArbStateRenderMetrics";
    }
}
