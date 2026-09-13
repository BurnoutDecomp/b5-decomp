#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (intro asserts)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // CgsCore::SPrintf
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"                // AllVehicleData (nearest race car)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                  // Camera::VehicleInfo (the race-car record)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer (gameplay cam)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "rw/math/vpu/types.h"                                                  // Matrix44Affine / Vector3
#include "rw/math/vpu/vector3_operation.h"                                      // Dot / Normalize / operator-
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::StringToKey / shotgroup

// ============================================================================
// BrnDirector::ArbStateRaceIntro -- Construct / GetName / Prepare / Update / Release.
//
// The director's "race intro" arbitrator state. On Prepare it picks the event's intro
// shot-group (default group from the resource manager, or an event-specific group built by
// name), allocates an ICE-anim camera behaviour to play it, and configures that behaviour.
// Update walks the intro state machine, copying the behaviour's produced camera into the
// state's own camera each frame and forcing the live gameplay camera to finish so the intro
// can take over; once the take has finished it hands control back to the roaming state.
// All member access is BY NAME; the GameState snapshot it reacts to (lrSharedInfo.mpGameState)
// is reached through the named BrnDirector::GameState members.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The vehicle-index selector GetNearestRaceCarIndexToPlayer is called with 1:
        // "race cars only" (vs. all tracked cars).
        const u32 KU_NEAREST_RACE_CARS_ONLY = 1u;

        // The event-specific-shot-group "none" sentinel (miEventSpecificShotGroup == -1 means
        // use the resource manager's default intro group).
        const s32 KI_NO_EVENT_SPECIFIC_SHOT_GROUP = -1;

        // The event modes that suppress the single-shot per-take reset byte in Prepare:
        // 15 and 16.
        const s32 KI_EVENT_TYPE_15 = 15;
        const s32 KI_EVENT_TYPE_16 = 16;

        // The event mode the COUNTDOWN-state "Car_Reset" effect gates on (event type == 7).
        const s32 KI_EVENT_TYPE_CAR_RESET = 7;

        // The blend the COUNTDOWN-state "Car_Reset" camera effect plays at.
        const f32 KF_CAR_RESET_BLEND = 1.0f;

        // The dirty-flag bit Update raises on the state's camera while an intro behaviour is
        // driving it (mCamera.mState_uFlags |= 2).
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;
    }

    // ------------------------------------------------------------------------
    // Construct -- build the camera, clear the base camera flags, and zero the
    // behaviour handle + state machine.
    // ------------------------------------------------------------------------
    void ArbStateRaceIntro::Construct()
    {
        GetNonConstCamera().Construct();   // the base camera at +0x10

        ResetBaseCameraFlags();            // clears the two flag bytes at +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // +0x194 = 0

        // The behaviour handle starts unallocated (+0x180 block zeroed: mbAllocated,
        // muAllocationKey, mpHelperPool, mpManager, mpBehaviour).
        mRaceIntroBehaviourHandle = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName
    // ------------------------------------------------------------------------
    const char* ArbStateRaceIntro::GetName() const
    {
        return "ArbStateRaceIntro";
    }

    // ------------------------------------------------------------------------
    // Prepare -- enter the race-intro state: pick the intro shot-group, allocate
    // and configure the ICE-anim behaviour. Only runs the setup when not already ACTIVE /
    // COUNTDOWN / CHANGING_TO_ROAMING and the behaviour is not already allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRaceIntro::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already running (ACTIVE_PRE_COUNTDOWN / ACTIVE_COUNTDOWN / CHANGING_TO_ROAMING): do
        // nothing (meState != 2 && != 3 && != 4).
        if (meState != E_STATE_ACTIVE_PRE_COUNTDOWN &&
            meState != E_STATE_ACTIVE_COUNTDOWN &&
            meState != E_STATE_CHANGING_TO_ROAMING)
        {
            const bool lbAlreadyAllocated = mRaceIntroBehaviourHandle.IsAllocated();

            meState = E_STATE_PREPARING;   // +0x194 = 1

            if (!lbAlreadyAllocated)
            {
                GameState& lrGameState = *lrSharedInfo.mpGameState;

                // ---- pick whether the nearest race car is "in front" of the player --------
                // The original takes the direction from the nearest race car to the player car,
                // normalises it, and compares its alignment with the player transform's At
                // (forward) axis against its alignment with the Right axis: the car is treated
                // as "in front" when it is more forward-aligned than side-aligned. That bool
                // selects which of the event's two intro shot-lists the resource manager hands
                // back.
                const AllVehicleData& lrAllVehicles = *lrSharedInfo.mpAllVehicleData;
                const EActiveRaceCarIndex leNearestRaceCar =
                    lrAllVehicles.GetNearestRaceCarIndexToPlayer(KU_NEAREST_RACE_CARS_ONLY);
                const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
                    *lrSharedInfo.mpPlayerCarTransform;

                // The race car's world position, by name: the console's lvx128 at record +0x220
                // is the car-to-world frame's translation row.
                const rw::math::vpu::Vector3& lrRaceCarPos =
                    lrAllVehicles.GetRaceCar(leNearestRaceCar).mRaceCarState.mTransform.wAxis;

                const rw::math::vpu::Vector3 lv3Dir =
                    rw::math::vpu::Normalize(lrRaceCarPos - lrPlayerTransform.Pos());
                const f32 lfAlongForward = rw::math::vpu::Dot(lv3Dir, lrPlayerTransform.At());
                const f32 lfAlongRight   = rw::math::vpu::Dot(lv3Dir, lrPlayerTransform.Right());
                const bool lbCarInFront  = lfAlongForward > lfAlongRight;

                // ---- resolve the intro shot-group -----------------------------------------
                // GetEventIntroShots returns the group BY REFERENCE off the real
                // DirectorResourceManager (it hands back `this + <group offset>`), so no cast
                // is needed here.
                const Attrib::Gen::shotgroup* lpDefaultShots =
                    &lrSharedInfo.mpDirectorResourceManager->GetEventIntroShots(
                        lrGameState.meEventType, lbCarInFront);

                if (lrGameState.miEventSpecificShotGroup == KI_NO_EVENT_SPECIFIC_SHOT_GROUP)
                {
                    mpRaceStartShotGroup = lpDefaultShots;
                }
                else
                {
                    // Build the event-specific group by its numeric id name ("%i").
                    char lacGroupName[32];
                    CgsCore::SPrintf(lacGroupName, 32, "%i", lrGameState.miEventSpecificShotGroup);
                    mShotGroup = Attrib::Gen::shotgroup(Attrib::StringToKey(lacGroupName), 0);
                    mpRaceStartShotGroup = &mShotGroup;
                }

                CGS_ASSERT(mpRaceStartShotGroup->Num_ShotList() > 0,
                           "mpRaceStartShotGroup->Num_ShotList()>0");

                // ---- allocate + configure the ICE-anim behaviour --------------------------
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mRaceIntroBehaviourHandle, this, 0, 1);

                const void* lpShotData = mpRaceStartShotGroup->GetShotListData(/*lbUseSecond*/ false);
                if (!lpShotData)
                    lpShotData = Attrib::DefaultDataArea(0x18u);

                Camera::BehaviourIceAnim* lpBehaviour = mRaceIntroBehaviourHandle.GetBehaviour();
                lpBehaviour->SetParameters(
                    static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                        const_cast<void*>(lpShotData)));
                lpBehaviour->SetForceLooseHeadingSpace(true);   // +0xE2A

                if (mpRaceStartShotGroup->Num_ShotList() <= 1)
                {
                    // Single shot: PAUSE the take unless the event mode is 15 or 16
                    // (behaviour +0xDE6 == the embedded KeyAnimController's mbPaused;
                    // RESOLVED 2026-08-05 -- the intro shot holds until the countdown).
                    if (lrGameState.meEventType != KI_EVENT_TYPE_15 &&
                        lrGameState.meEventType != KI_EVENT_TYPE_16)
                    {
                        mRaceIntroBehaviourHandle.GetBehaviour()->SetTakePaused(true);  // +0xDE6
                    }
                }
                else
                {
                    // Multiple shots: the bound take loops (behaviour +0xDE4 == the embedded
                    // KeyAnimController's mbIsLooping; RESOLVED 2026-08-05).
                    mRaceIntroBehaviourHandle.GetBehaviour()->SetTakeLooping(true);      // +0xDE4
                }

                mRaceIntroBehaviourHandle.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
                mRaceIntroBehaviourHandle.GetBehaviour()->ClearBaseFirstFrameGate();     // base +0x28 = 0
            }
        }

        return true;
    }

    // ------------------------------------------------------------------------
    // Update -- per-frame intro state machine.
    // ------------------------------------------------------------------------
    void ArbStateRaceIntro::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera    = GetNonConstCamera();
        GameState&      lrGameState = *lrSharedInfo.mpGameState;

        lrCamera.Construct();   // the base camera at +0x10, at entry

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
        {
            // Run Prepare; on success force the live gameplay camera to finish so the intro
            // camera can take over, then advance to ACTIVE_PRE_COUNTDOWN.
            if (Prepare(lrSharedInfo))
            {
                lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
                meState = E_STATE_ACTIVE_PRE_COUNTDOWN;   // +0x194 = 2
            }
            break;
        }

        case E_STATE_ACTIVE_PRE_COUNTDOWN:
        {
            // Drive the state camera from the behaviour and mark it behaviour-driven.
            lrCamera = mRaceIntroBehaviourHandle.GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            // Once the event reaches the countdown, either resume the single-shot take and
            // advance, or (for multi-shot groups) re-allocate the behaviour for the countdown
            // take before advancing.
            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_COUNTDOWN)
            {
                if (mpRaceStartShotGroup->Num_ShotList() <= 1)
                {
                    mRaceIntroBehaviourHandle.GetBehaviour()->SetTakePaused(false);   // +0xDE6 = 0
                    meState = E_STATE_ACTIVE_COUNTDOWN;   // +0x194 = 3
                }
                else
                {
                    lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                        mRaceIntroBehaviourHandle, this, 0, 1);

                    const void* lpShotData =
                        mpRaceStartShotGroup->GetShotListData(/*lbUseSecond*/ true);
                    if (!lpShotData)
                        lpShotData = Attrib::DefaultDataArea(0x18u);

                    Camera::BehaviourIceAnim* lpBehaviour = mRaceIntroBehaviourHandle.GetBehaviour();
                    lpBehaviour->SetParameters(
                        static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                            const_cast<void*>(lpShotData)));
                    lpBehaviour->SetUseCollisionPolicy(true);       // +0xE28
                    mRaceIntroBehaviourHandle.GetBehaviour()->ClearBaseFirstFrameGate(); // base +0x28 = 0
                    mRaceIntroBehaviourHandle.GetBehaviour()->SetForceLooseHeadingSpace(true); // +0xE2A
                    meState = E_STATE_ACTIVE_COUNTDOWN;   // +0x194 = 3
                }
            }
            break;
        }

        case E_STATE_ACTIVE_COUNTDOWN:
        {
            lrCamera = mRaceIntroBehaviourHandle.GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            if (lrGameState.meEventType == KI_EVENT_TYPE_CAR_RESET)
            {
                if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_ACTIVE)
                {
                    Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                                  "Car_Reset", KF_CAR_RESET_BLEND);
                }
                // Force the live gameplay camera to finish so the intro keeps control.
                lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
            }

            if (mRaceIntroBehaviourHandle.GetBehaviour()->HasFinishedOrFailed())
            {
                const bool lbReleased = mRaceIntroBehaviourHandle.Release();
                CGS_ASSERT(lbReleased, "mRaceIntroBehaviourHandle.Release()");

                ArbUtils::ChangeToState<EState>(
                    this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                    meState, E_STATE_CHANGING_TO_ROAMING);
            }
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
        {
            // Latch the currently-selected gameplay camera, then hand control back to roaming.
            lrCamera = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();

            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        // After any state work: if we are not INACTIVE and the player just joined freeburn,
        // abort the intro back to roaming.
        if (meState != E_STATE_INACTIVE &&
            lrGameState.mbStartingFreeburnDueToPlayerJoinThisFrame)
        {
            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
        }
    }

    // ------------------------------------------------------------------------
    // Release -- leave the race-intro state: reset the state machine, release the
    // ICE-anim behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRaceIntro::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x194 = 0

        // The handle release the console inlines here is the shared handle's own Release():
        // when allocated, UnSetBehaviourUsedByHandle(muAllocationKey) on the owning manager,
        // then zero the five-word block (+0x180).
        mRaceIntroBehaviourHandle.Release();

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
