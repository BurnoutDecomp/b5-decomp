#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineCarSelect.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (online car-select asserts)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Camera/Camera.h"                                  // BrnDirector::Camera::Camera (mTransform clamp)
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"               // NamedParameters (look-around-car params)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying / StopCurrentEffect
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim + DirectorResourceManager slice
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // Camera::BehaviourRotateAboutVehicle
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup + Attrib::DefaultDataArea
#include "rw/math/vpu/types.h"                                                  // rw::math::vpu::Vector3 (player-car clamp)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"          // Camera::VehicleInfo (mpPlayerCar, by name)

// ============================================================================
// BrnDirector::ArbStateOnlineCarSelect -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct  @0x8225B3E0
//   GetName    @0x821F6730
//   Prepare    @0x82271020
//   Update     @0x82236840
//   Release    @0x82236E28
//
// The director's ONLINE car-select / livery arbitrator state. On Prepare it allocates two
// camera behaviours -- an ICE-anim "reveal" cam (the black-and-white car reveal) and a
// rotate-about-vehicle "look around car" cam -- and configures each from its parameter block.
// Update walks the car-select state machine, each frame copying one behaviour's produced
// camera into the state's own camera and driving the "Black_Out_BW" / "Black_In_BW" /
// "Black_*(No_B_W)" / "BlackFadeIn_Quick" screen-fade hooks, then either holds, advances, or
// hands control to the online race-intro state (when the player is ready and the game mode can
// start a race intro) or back to the roaming state (when the car-select is aborted). After any
// state work, while the game asks the car-select to clamp to the car, it snaps the state
// camera's position onto the player car.
//
// All member access is BY NAME. The GameState snapshot it reacts to (lrSharedInfo.mpGameState)
// is reached through the named BrnDirector::GameState members.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The default attrib data-area size requested when a shot's parameter block is absent
        // (X360 li r3, 0x18 -> Attrib::DefaultDataArea(0x18)). Same as the race-intro/rank-up states.
        const u32 KU_SHOT_DEFAULT_DATA_AREA_SIZE = 0x18u;

        // The two trailing selectors the BehaviourManager::NewBehaviour<TBehaviour> allocation
        // request carries (X360 li r6,0 / li r7,1). Same as the sibling arbitrator states.
        // r6 is the manager's `const void* lpOwner` slot (null here -- the arbitrator states own
        // their behaviours through lpOwningState), so it must be typed as a POINTER: a `const s32`
        // whose value is 0 stopped being a null-pointer constant in C++11, so the old `s32` form
        // matched no NewBehaviour overload under /std:c++17 /permissive-. ArbStateDriveThru
        // already spells it `const void* const`; matched here.
        const void* const KI_NEW_BEHAVIOUR_ARG_A = 0;
        const s32         KI_NEW_BEHAVIOUR_ARG_B = 1;

        // The reveal take LOOPS. RESOLVED 2026-08-05: the X360's `stb 1` at behaviour +0xDE4
        // is the embedded KeyAnimController's own mbIsLooping (+0x764), not a "reset byte".
        const bool KB_TAKE_LOOPS = true;

        // The blend every car-select screen-fade camera-PFX hook plays at (flt_82001C98 == 1.0).
        const f32 KF_FADE_BLEND = 1.0f;

        // The parametric time the reveal take is rewound to when the car-select re-enters the
        // SELECTING_CAR state from the livery screen (flt_82001CC0 == 0.0).
        const f32 KF_TAKE_START_PARAMETRIC_TIME = 0.0f;

        // The screen-fade camera-PFX hook names the state requests.
        const char* const KPC_HOOK_BLACK_OUT_BW       = "Black_Out_BW";
        const char* const KPC_HOOK_BLACK_OUT_NO_BW    = "Black_Out(No_B_W)";
        const char* const KPC_HOOK_BLACK_IN_BW        = "Black_In_BW";
        const char* const KPC_HOOK_BLACK_IN_NO_BW     = "Black_In(No_B_W)";
        const char* const KPC_HOOK_BLACK_FADE_IN_QUICK = "BlackFadeIn_Quick";

        // The game modes (GameState::meEventType, EGameModeType) the car-select treats as
        // "online race" modes that may start a race intro: 10, 11, 12, 13, 14, 17. X360 reads
        // these as the meEventType cmpwi set in Update's transition gate (0xA/0xB/0xC/0xD/0xE/
        // 0x11). FLAG: the exact EGameModeType enumerator names are not recovered for this TU;
        // the VALUES are asm-attested.
        bool CanStartOnlineRaceIntro(s32 liEventType)
        {
            return liEventType == 10 || liEventType == 11 || liEventType == 12 ||
                   liEventType == 13 || liEventType == 14 || liEventType == 17;
        }
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::GetProducedCamera -- the camera the live behaviour produced this frame
    // (the manager keeps it alongside the behaviour; the X360 reads slot+0x10). Defined
    // out-of-line where the behaviour type is complete. The ICE-anim behaviour exposes it by
    // name; the rotate-about-vehicle behaviour has no reconstructed home for it yet, so its
    // produced camera is reached through the manager pool slot the handle resolves to (modelled
    // here as the same role). FLAG: the rotate-about-vehicle GetProducedCamera path is the
    // minimal slice -- the camera ROLE is reproduced (the X360 sub_821FDF38 reads slot+0x10).
    // ------------------------------------------------------------------------
    template <>
    const Camera::Camera&
    ArbStateOnlineCarSelect::BehaviourHandle<Camera::BehaviourIceAnim>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return mpBehaviour->GetProducedCamera();
    }

    template <>
    const Camera::Camera&
    ArbStateOnlineCarSelect::BehaviourHandle<Camera::BehaviourRotateAboutVehicle>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        // The rotate-about-vehicle behaviour's produced camera lives at the same manager-pool
        // slot offset the ICE-anim behaviour's does (the X360 sub_821FDF38 reads slot+0x10);
        // reached BY NAME through the behaviour's GetProducedCamera() accessor.
        return mpBehaviour->GetProducedCamera();
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::IsBehaviourWaitingToPrepare -- the X360's "is the take still queued for
    // its initial Prepare?" query (sub @0x822128A0): assert allocated, then ask the manager. The
    // online-car-select Prepare returns this value DIRECTLY (no negation, unlike the rank-up
    // state). Defined out-of-line where BehaviourManager is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    bool ArbStateOnlineCarSelect::BehaviourHandle<TBehaviour>::IsBehaviourWaitingToPrepare() const
    {
        CGS_ASSERT(mbAllocated, "mbIsAllocated");
        return mpManager->IsBehaviourWaitingToPrepare(muAllocationKey);
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225B3E0 -- build the camera, clear the base camera flags, zero the state
    // machine, and zero both behaviour handles. (mbWasCarModScreen is left for Update to seed;
    // the X360 Construct does not write +0x1A8.)
    // ------------------------------------------------------------------------
    void ArbStateOnlineCarSelect::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // +0x1AC = 0

        // Both behaviour handles start unallocated (+0x180 / +0x194 blocks zeroed: mbAllocated,
        // muAllocationKey, muHelperIndex, mpManager, mpBehaviour).
        mIceCam           = BehaviourHandle<Camera::BehaviourIceAnim>();
        mLookAroundCarCam = BehaviourHandle<Camera::BehaviourRotateAboutVehicle>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6730
    // ------------------------------------------------------------------------
    const char* ArbStateOnlineCarSelect::GetName() const
    {
        return "ArbStateOnlineCarSelect";
    }

    // ------------------------------------------------------------------------
    // Prepare @0x82271020 -- enter the car-select state: allocate and configure both camera
    // behaviours (the ICE-anim reveal cam and the rotate-about-vehicle look-around cam). Does
    // nothing once already PREPARING-or-later (meState != 0, i.e. mbPreparing latch +0x1AC).
    // Returns whether the ICE-anim reveal behaviour is still waiting to prepare.
    // ------------------------------------------------------------------------
    bool ArbStateOnlineCarSelect::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // The X360 latches meState (+0x1AC) as a "have we prepared this cycle" gate: when it is
        // already non-zero, do nothing and report ready (return 1). Otherwise set it and run the
        // allocation.
        if (meState != E_STATE_INACTIVE)
        {
            return true;
        }

        const bool lbIceCamAlreadyAllocated = mIceCam.IsAllocated();

        meState = E_STATE_PREPARING;   // +0x1AC = 1

        if (!lbIceCamAlreadyAllocated)
        {
            // ---- the ICE-anim "reveal" cam --------------------------------------------------
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mIceCam, this, KI_NEW_BEHAVIOUR_ARG_A, KI_NEW_BEHAVIOUR_ARG_B);

            const void* lpShotData =
                lrSharedInfo.mpDirectorResourceManager->GetOnlineCarSelect().GetShotListData(
                    /*lbUseSecond*/ false);
            if (!lpShotData)
                lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

            Camera::BehaviourIceAnim* lpIceBehaviour = mIceCam.GetBehaviour();
            lpIceBehaviour->SetParameters(
                static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                    const_cast<void*>(lpShotData)));
            mIceCam.GetBehaviour()->SetTakeLooping(KB_TAKE_LOOPS);   // +0xDE4 = 1
        }

        if (!mLookAroundCarCam.IsAllocated())
        {
            // ---- the rotate-about-vehicle "look around car" cam -----------------------------
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourRotateAboutVehicle>(
                mLookAroundCarCam, this, KI_NEW_BEHAVIOUR_ARG_A, KI_NEW_BEHAVIOUR_ARG_B);

            // Configure it from the "look around car" named-parameter block (NamedParameters
            // +0x2334).
            const NamedParameters::LookAroundCarCamParameters& lrParams =
                lrSharedInfo.mpNamedParameters->GetLookAroundCarCamParameters();
            mLookAroundCarCam.GetBehaviour()->SetParameters(&lrParams);
        }

        // The X360 tail-returns the reveal behaviour's "still waiting to prepare?" query directly
        // (sub @0x822128A0; no negation).
        return mIceCam.IsBehaviourWaitingToPrepare();
    }

    // ------------------------------------------------------------------------
    // Update @0x82236840 -- per-frame car-select state machine.
    //
    // mbWasCarModScreen selects, in the later states, which behaviour's produced camera to copy
    // and which fade-hook variant to play: when set (the car-mod / livery screen was entered)
    // the look-around-car cam + the "(No_B_W)" hooks; when clear the ICE reveal cam + the "_BW"
    // hooks.
    // ------------------------------------------------------------------------
    void ArbStateOnlineCarSelect::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera          = GetNonConstCamera();
        GameState&      lrGameState       = *lrSharedInfo.mpGameState;
        const EffectInterface& lrEffects  = *lrSharedInfo.mpEffectInterface;

        switch (meState)
        {
        case E_STATE_INACTIVE:
            // X360: case 0 jumps straight to the epilogue, skipping the clamp-to-car block.
            return;

        case E_STATE_PREPARING:
            // Run Prepare; on a non-ready result (still preparing) stop here this frame. On
            // success advance to SELECTING_CAR and fall into the case-2 body that frame (the
            // X360 case-1 success edge sets +0x1AC = 2 and falls into case 2).
            if (!Prepare(lrSharedInfo))
            {
                break;
            }
            meState = E_STATE_SELECTING_CAR;   // +0x1AC = 2
            [[fallthrough]];

        case E_STATE_SELECTING_CAR:
        {
            // Drive the state camera from the ICE-anim reveal cam and fade the screen out (B&W).
            lrCamera = mIceCam.GetProducedCamera();
            Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_OUT_BW, KF_FADE_BLEND);

            // In the INTRO event state with the game saying the car-select can start the intro,
            // hand over to the online race-intro state IFF the game mode is an online-race mode;
            // otherwise hold this frame. (X360: when the INTRO && canStart gate fails, fall to
            // the abort / finished / car-mod checks instead.)
            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_INTRO &&
                lrGameState.mbOnlineCarSelectCanStartRaceIntro)
            {
                if (CanStartOnlineRaceIntro(lrGameState.meEventType))
                {
                    ArbUtils::ChangeToState<EState>(
                        this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                        meState, E_STATE_CHANGING_TO_INTRO);
                }
            }
            else if (lrGameState.mbHasOnlineCarSelectBeenAborted)
            {
                // Aborted: latch "clamp to car", stop the effect, and hand back to roaming.
                mbWasCarModScreen = true;   // +0x1A8 = 1
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                ArbUtils::ChangeToState<EState>(
                    this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                    meState, E_STATE_CHANGING_TO_ROAMING);
            }
            else if (!lrGameState.mbIsOnlineCarSelectActive)
            {
                // Car-select finished: fade the screen back in (B&W) and wait to change to intro.
                meState = E_STATE_WAIT_TO_CHANGE_TO_INTRO;   // +0x1AC = 4
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_BW, KF_FADE_BLEND);
                mbWasCarModScreen = false;   // +0x1A8 = 0
            }
            else if (lrGameState.mbJunkyardCarModActive)
            {
                // The player opened the car-mod (livery) screen: go to the livery state and fade
                // out (non-B&W, since the look-around cam takes over there).
                meState = E_STATE_SELECTING_LIVERY;   // +0x1AC = 3
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_OUT_NO_BW, KF_FADE_BLEND);
            }
            break;
        }

        case E_STATE_SELECTING_LIVERY:
        {
            // Drive the state camera from the look-around-car cam and stop the current effect.
            lrCamera = mLookAroundCarCam.GetProducedCamera();
            Camera::StopCurrentEffect(lrCamera, lrEffects);

            // Unless the car is showable, re-arm the quick black fade-in.
            if (!lrGameState.mbOnlineCarSelectCarIsShowable)
            {
                Camera::RequestStartEffectHookReset(lrCamera, KPC_HOOK_BLACK_FADE_IN_QUICK, KF_FADE_BLEND);
            }

            // In the INTRO event state with the game saying the car-select can start the intro,
            // hand over to the online race-intro state IFF the game mode is an online-race mode
            // (stopping the effect first); otherwise hold. (X360: when the INTRO && canStart gate
            // fails, fall to the abort / finished / car-mod checks instead.)
            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_INTRO &&
                lrGameState.mbOnlineCarSelectCanStartRaceIntro)
            {
                if (CanStartOnlineRaceIntro(lrGameState.meEventType))
                {
                    Camera::StopCurrentEffect(lrCamera, lrEffects);
                    ArbUtils::ChangeToState<EState>(
                        this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                        meState, E_STATE_CHANGING_TO_INTRO);
                }
            }
            else if (lrGameState.mbHasOnlineCarSelectBeenAborted)
            {
                // Aborted: latch "clamp to car", stop the effect, and hand back to roaming.
                mbWasCarModScreen = true;   // +0x1A8 = 1
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                ArbUtils::ChangeToState<EState>(
                    this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                    meState, E_STATE_CHANGING_TO_ROAMING);
            }
            else if (!lrGameState.mbIsOnlineCarSelectActive)
            {
                // Car-select finished: fade in (non-B&W), latch "clamp to car".
                meState = E_STATE_WAIT_TO_CHANGE_TO_INTRO;   // +0x1AC = 4
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_NO_BW, KF_FADE_BLEND);
                mbWasCarModScreen = true;   // +0x1A8 = 1
            }
            else if (!lrGameState.mbJunkyardCarModActive)
            {
                // Left the car-mod (livery) screen: back to SELECTING_CAR, rewind the reveal take
                // to its start.
                meState = E_STATE_SELECTING_CAR;   // +0x1AC = 2
                mIceCam.GetBehaviour()->SetControllerParametricTime0To1(KF_TAKE_START_PARAMETRIC_TIME);
            }
            break;
        }

        case E_STATE_WAIT_TO_CHANGE_TO_INTRO:
        {
            // Drive the state camera from whichever cam is in force and fade the screen in.
            if (mbWasCarModScreen)
            {
                lrCamera = mLookAroundCarCam.GetProducedCamera();
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_NO_BW, KF_FADE_BLEND);
            }
            else
            {
                lrCamera = mIceCam.GetProducedCamera();
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_BW, KF_FADE_BLEND);
            }

            // In the INTRO event state with the game saying the car-select can start the intro,
            // hand over to the online race-intro state IFF the game mode is an online-race mode;
            // otherwise hold. (X360: when the INTRO && canStart gate fails, fall to the abort
            // check instead.)
            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_INTRO &&
                lrGameState.mbOnlineCarSelectCanStartRaceIntro)
            {
                if (CanStartOnlineRaceIntro(lrGameState.meEventType))
                {
                    ArbUtils::ChangeToState<EState>(
                        this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                        meState, E_STATE_CHANGING_TO_INTRO);
                }
            }
            else if (lrGameState.mbHasOnlineCarSelectBeenAborted)
            {
                // Aborted: latch "clamp to car", stop the effect, and hand back to roaming.
                mbWasCarModScreen = true;   // +0x1A8 = 1
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                ArbUtils::ChangeToState<EState>(
                    this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                    meState, E_STATE_CHANGING_TO_ROAMING);
            }
            break;
        }

        case E_STATE_CHANGING_TO_INTRO:
        {
            // Keep driving the camera + fade while the hand-off to the online race intro
            // completes, then change to it.
            if (mbWasCarModScreen)
            {
                lrCamera = mLookAroundCarCam.GetProducedCamera();
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_NO_BW, KF_FADE_BLEND);
            }
            else
            {
                lrCamera = mIceCam.GetProducedCamera();
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, KPC_HOOK_BLACK_IN_BW, KF_FADE_BLEND);
            }

            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                meState, E_STATE_CHANGING_TO_INTRO);
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
        {
            // Keep driving the camera while the hand-off to roaming completes, stop the effect,
            // then change to roaming.
            if (mbWasCarModScreen)
            {
                lrCamera = mLookAroundCarCam.GetProducedCamera();
            }
            else
            {
                lrCamera = mIceCam.GetProducedCamera();
            }
            Camera::StopCurrentEffect(lrCamera, lrEffects);

            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
            break;
        }

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }

        // After any state work: while the game asks the car-select to clamp to the car, snap the
        // state camera's position onto the player car. The X360 copies a single 16-byte lane
        // from the player-car VehicleInfo (+0x220) into the camera transform's translation row
        // (mCamera.mTransform.Pos()).
        if (lrGameState.mbOnlineCarSelectMustClampToCar)   // GameState +0x1A8
        {
            // ✅ THE OFFSET HACK IS GONE (2026-08-29, crash-camera wave). The +0x220 the
            // console loads is mRaceCarState.mTransform (@496 == 0x1F0) + 0x30, i.e. the
            // transform's position row -- read here BY NAME now that mpPlayerCar is typed
            // `const Camera::VehicleInfo*` instead of the retired BrnDirector::VehicleInfo
            // namespace fork.
            const rw::math::vpu::Vector3& lrPlayerCarPos =
                lrSharedInfo.mpPlayerCar->mRaceCarState.mTransform.Pos();

            lrCamera.mTransform.Pos() = lrPlayerCarPos;   // X360 stvx128 v0, this+0x40 (== mCamera.mTransform +0x30)
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82236E28 -- leave the car-select state: reset the state machine, release both
    // camera behaviours back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateOnlineCarSelect::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x1AC = 0

        if (mIceCam.mbAllocated)      // +0x180 block
        {
            mIceCam.mpManager->UnSetBehaviourUsedByHandle(mIceCam.muAllocationKey);
            mIceCam.muHelperIndex = 0;
            mIceCam.mpManager     = 0;
            mIceCam.mpBehaviour   = 0;
            mIceCam.mbAllocated   = false;
        }

        if (mLookAroundCarCam.mbAllocated)   // +0x194 block
        {
            mLookAroundCarCam.mpManager->UnSetBehaviourUsedByHandle(mLookAroundCarCam.muAllocationKey);
            mLookAroundCarCam.muHelperIndex = 0;
            mLookAroundCarCam.mpManager     = 0;
            mLookAroundCarCam.mpBehaviour   = 0;
            mLookAroundCarCam.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
