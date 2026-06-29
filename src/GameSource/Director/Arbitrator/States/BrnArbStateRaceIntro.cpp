#include "GameSource/Director/Arbitrator/States/BrnArbStateRaceIntro.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (intro asserts)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                         // CgsCore::SPrintf
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"                // AllVehicleData (nearest race car)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer (gameplay cam)
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "rw/math/vpu/types.h"                                                  // Matrix44Affine / Vector3
#include "rw/math/vpu/vector3_operation.h"                                      // Dot / Normalize / operator-
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::StringToKey / shotgroup

// ============================================================================
// BrnDirector::ArbStateRaceIntro -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x8225ADB8
//   GetName    @0x821F6320
//   Prepare    @0x8226E350
//   Update     @0x8226E5B0
//   Release    @0x82235D50
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
        // The vehicle-index selector GetNearestRaceCarIndexToPlayer is called with (X360 li
        // r4, 1): "race cars only" (vs. all tracked cars).
        const u32 KU_NEAREST_RACE_CARS_ONLY = 1u;

        // The event-specific-shot-group "none" sentinel (miEventSpecificShotGroup == -1 means
        // use the resource manager's default intro group).
        const s32 KI_NO_EVENT_SPECIFIC_SHOT_GROUP = -1;

        // The event modes that suppress the single-shot per-take reset byte (X360 cmpwi 0xF /
        // 0x10 in Prepare): 15 and 16.
        const s32 KI_EVENT_TYPE_15 = 15;
        const s32 KI_EVENT_TYPE_16 = 16;

        // The event mode the COUNTDOWN-state "Car_Reset" effect gates on (X360 evType == 7).
        const s32 KI_EVENT_TYPE_CAR_RESET = 7;

        // The blend the COUNTDOWN-state "Car_Reset" camera effect plays at (flt_82001C98 == 1.0).
        const f32 KF_CAR_RESET_BLEND = 1.0f;

        // The dirty-flag bit Update raises on the state's camera while an intro behaviour is
        // driving it (X360 mCamera.mState_uFlags |= 2).
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::Release @ sub_8222DFD8 -- drop the manager-side hold on the behaviour
    // and clear the handle. Defined out-of-line where the BehaviourManager type is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    bool ArbStateRaceIntro::BehaviourHandle<TBehaviour>::Release()
    {
        if (mbAllocated)
        {
            mpManager->UnSetBehaviourUsedByHandle(muAllocationKey);
            muHelperIndex   = 0;
            mpManager       = 0;
            mpBehaviour     = 0;
            mbAllocated     = false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::GetProducedCamera -- the camera the live ICE-anim behaviour produced
    // this frame (the manager keeps it alongside the behaviour; modelled here BY NAME as the
    // behaviour's produced camera). Defined out-of-line where BehaviourIceAnim is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    const Camera::Camera& ArbStateRaceIntro::BehaviourHandle<TBehaviour>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return mpBehaviour->GetProducedCamera();
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225ADB8 -- build the camera, clear the base camera flags, and zero the
    // behaviour handle + state machine.
    // ------------------------------------------------------------------------
    void ArbStateRaceIntro::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // +0x194 = 0

        // The behaviour handle starts unallocated (+0x180 block zeroed: mbAllocated,
        // muAllocationKey, muHelperIndex, mpManager, mpBehaviour).
        mRaceIntroBehaviourHandle = BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6320
    // ------------------------------------------------------------------------
    const char* ArbStateRaceIntro::GetName() const
    {
        return "ArbStateRaceIntro";
    }

    // ------------------------------------------------------------------------
    // Prepare @0x8226E350 -- enter the race-intro state: pick the intro shot-group, allocate
    // and configure the ICE-anim behaviour. Only runs the setup when not already ACTIVE /
    // COUNTDOWN / CHANGING_TO_ROAMING and the behaviour is not already allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRaceIntro::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already running (ACTIVE_PRE_COUNTDOWN / ACTIVE_COUNTDOWN / CHANGING_TO_ROAMING): do
        // nothing. (X360: meState != 2 && != 3 && != 4.)
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
                // The X360 takes the direction from the nearest race car to the player car,
                // normalises it, and compares its alignment with the player transform's At
                // (forward) axis against its alignment with the Right axis: the car is treated
                // as "in front" when it is more forward-aligned than side-aligned. That bool
                // selects which of the event's two intro shot-lists the resource manager hands
                // back.
                const AllVehicleData& lrAllVehicles = *lrSharedInfo.mpAllVehicleData;
                const s32 liNearestRaceCar =
                    lrAllVehicles.GetNearestRaceCarIndexToPlayer(KU_NEAREST_RACE_CARS_ONLY);
                const void* lpRaceCar = lrAllVehicles.GetRaceCar(liNearestRaceCar);

                const rw::math::vpu::Matrix44Affine& lrPlayerTransform =
                    *static_cast<const rw::math::vpu::Matrix44Affine*>(lrSharedInfo.mpPlayerCarTransform);
                // The race car's position is the lvx128 at race-car +0x220 (a Vector3 lane).
                const rw::math::vpu::Vector3& lrRaceCarPos =
                    *reinterpret_cast<const rw::math::vpu::Vector3*>(
                        static_cast<const u8*>(lpRaceCar) + 0x220);

                const rw::math::vpu::Vector3 lv3Dir =
                    rw::math::vpu::Normalize(lrRaceCarPos - lrPlayerTransform.Pos());
                const f32 lfAlongForward = rw::math::vpu::Dot(lv3Dir, lrPlayerTransform.At());
                const f32 lfAlongRight   = rw::math::vpu::Dot(lv3Dir, lrPlayerTransform.Right());
                const bool lbCarInFront  = lfAlongForward > lfAlongRight;   // X360 vcmpgtfp

                // ---- resolve the intro shot-group -----------------------------------------
                const Attrib::Gen::shotgroup* lpDefaultShots =
                    static_cast<const Attrib::Gen::shotgroup*>(
                        lrSharedInfo.mpDirectorResourceManager->GetEventIntroShots(
                            lrGameState.meEventType, lbCarInFront));

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
                    // Single shot: seed reset byte 2 unless the event mode is 15 or 16.
                    if (lrGameState.meEventType != KI_EVENT_TYPE_15 &&
                        lrGameState.meEventType != KI_EVENT_TYPE_16)
                    {
                        mRaceIntroBehaviourHandle.GetBehaviour()->SetTakeResetByte2(1);  // +0xDE6
                    }
                }
                else
                {
                    // Multiple shots: seed reset byte 0.
                    mRaceIntroBehaviourHandle.GetBehaviour()->SetTakeResetByte0(1);      // +0xDE4
                }

                mRaceIntroBehaviourHandle.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
                mRaceIntroBehaviourHandle.GetBehaviour()->ClearBaseFirstFrameGate();     // base +0x28 = 0
            }
        }

        return true;
    }

    // ------------------------------------------------------------------------
    // Update @0x8226E5B0 -- per-frame intro state machine.
    // ------------------------------------------------------------------------
    void ArbStateRaceIntro::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera    = GetNonConstCamera();
        GameState&      lrGameState = *lrSharedInfo.mpGameState;

        lrCamera.Construct();   // X360 Camera::Construct(this+0x10) at entry

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

            // Once the event reaches the countdown, either drop the single-shot reset byte and
            // advance, or (for multi-shot groups) re-allocate the behaviour for the countdown
            // take before advancing.
            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_COUNTDOWN)
            {
                if (mpRaceStartShotGroup->Num_ShotList() <= 1)
                {
                    mRaceIntroBehaviourHandle.GetBehaviour()->SetTakeResetByte2(0);   // +0xDE6 = 0
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
    // Release @0x82235D50 -- leave the race-intro state: reset the state machine, release the
    // ICE-anim behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRaceIntro::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x194 = 0

        if (mRaceIntroBehaviourHandle.IsAllocated())   // +0x180 block
        {
            mRaceIntroBehaviourHandle.mpManager->UnSetBehaviourUsedByHandle(
                mRaceIntroBehaviourHandle.muAllocationKey);
            mRaceIntroBehaviourHandle.muHelperIndex = 0;
            mRaceIntroBehaviourHandle.mpManager     = 0;
            mRaceIntroBehaviourHandle.mpBehaviour   = 0;
            mRaceIntroBehaviourHandle.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
