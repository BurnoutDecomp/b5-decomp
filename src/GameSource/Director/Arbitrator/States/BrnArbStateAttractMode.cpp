#include "GameSource/Director/Arbitrator/States/BrnArbStateAttractMode.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT (unhandled-state assert)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::GetState/SetCurrentState
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                 // Camera::BehaviourManager (complete)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRoadRunner.h"   // Camera::BehaviourRoadRunner (slice; GetCamera)
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
// handle's named GetCamera() accessor (mirrors ArbStateRoaming / ArbStateCrashNav), and
// the roaming hand-off goes through the state container's named GetState()/SetCurrentState().
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    // (The two out-of-line BehaviourHandle<> template bodies this file used to carry -- for
    // this state's own nested handle copy -- are gone: the state now uses the SHARED
    // Camera::BehaviourHandle<>, whose GetCamera / IsBehaviourWaitingToPrepare are bodied in
    // BrnBehaviourManager.h against the manager's real helper pool.)

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
        mRoadRunnerCam.Construct();
    }

    // ------------------------------------------------------------------------
    // ⭐ Prepare @0x8225B220 -- bring the fly-by up, and report whether it is ready to drive.
    // THE GATE IS LIFTED. This function is the one the whole two-wave campaign was aimed at.
    //
    //     if ( meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING )
    //         return true;                                   // already up
    //     meState = E_STATE_PREPARING;
    //     if ( !mRoadRunnerCam.IsAllocated() )
    //         lrSharedInfo.mpBehaviourManager->NewBehaviour<BehaviourRoadRunner>(
    //             mRoadRunnerCam, this, 0, 1 );              // owner = this state, refLimit 1
    //     return !mRoadRunnerCam.IsBehaviourWaitingToPrepare();
    //
    // The X360 schedules the mbAllocated read before the meState store and allocates only when the handle is
    // empty, so a repeated Prepare is idempotent -- it just re-asks the manager whether the
    // behaviour has finished its first Prepare yet. That polling shape is why
    // Arbitrator::Update's CHANGING_TO_ATTRACT_MODE case can call this every frame.
    //
    // HOW THE POLL NOW TERMINATES (it never could before):
    //   frame N   : NewBehaviour<> allocates a helper slot, constructs a BehaviourRoadRunner in
    //               one of the manager's behaviour pools, raises mBehaviourNeedsPreparingFlags
    //               for that slot and binds this handle to it. IsBehaviourWaitingToPrepare() reads that
    //               same bit -> true -> Prepare returns FALSE.
    //   frame N   : (tail of MainDirector::Update) BehaviourManager::PrepareBehaviours walks the
    //               needs-preparing set, dispatches BehaviourRoadRunner::Prepare -- which is a
    //               pure field sweep that ALWAYS returns true -- and clears the bit.
    //   frame N+1 : IsBehaviourWaitingToPrepare() -> false -> Prepare returns TRUE, Arbitrator::Update's
    //               CHANGING_TO_ATTRACT_MODE case advances to E_STATE_ATTRACT_MODE, and this
    //               state's own Update moves to E_STATE_ACTIVE.
    // ⚠️ That second step only happens if the conductor calls PrepareBehaviours -- the console
    // does it at the tail of MainDirector::Update (line 878), which this wave wires up.
    // ------------------------------------------------------------------------
    bool ArbStateAttractMode::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
            return true;

        meState = E_STATE_PREPARING;   // +0x194 = 1

        if (!mRoadRunnerCam.IsAllocated())
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourRoadRunner>(
                mRoadRunnerCam, this, 0, 1);
        }

        // ARTIST 0x8225B278 calls the out-of-line IsBehaviourWaitingToPrepare copy (0x82212AC8) and
        // negates its bool (clrlwi/cntlzw); DWARF BrnArbStateAttractMode.cpp:49 keeps the
        // answer in the local lbPrepared.
        const bool lbPrepared = !mRoadRunnerCam.IsBehaviourWaitingToPrepare();
        return lbPrepared;
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
            GetNonConstCamera() = mRoadRunnerCam.GetCamera();
            break;

        case E_STATE_CHANGING_TO_ROAMING:
        {
            GetNonConstCamera() = mRoadRunnerCam.GetCamera();

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
            CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("unhandled state", "..\\..\\..\\GameSource\\Director/Arbitrator/States/BrnArbStateAttractMode.cpp", 130));
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

        // The X360 inlines BehaviourHandle::Release @0x8222DD00 here (the same
        // UnSetBehaviourUsedByHandle + four-field clear); the shared handle owns that body now.
        mRoadRunnerCam.Release();

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
