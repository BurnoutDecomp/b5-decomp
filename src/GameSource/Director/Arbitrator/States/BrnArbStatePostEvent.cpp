#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (post-event asserts)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim + DirectorResourceManager slice
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup
// NOTE: Attrib::Gen::iceanim (PickAppropriateShot's return / BehaviourIceAnim::ShotReference)
// is defined by BrnBehaviourIceAnim.h above -- do NOT also include the generated iceanim.h
// (that would be a duplicate definition of Attrib::Gen::iceanim in this TU).

// NOTE: the director-camera cone uses the minimal DirectorResourceManager SLICE declared in
// BrnBehaviourIceAnim.h (GetEventCompletionShots / GetPostEventMovieData), NOT the heavier
// BrnDirectorResourceManager.h home -- including both in one TU would be an ODR conflict (two
// definitions of BrnDirector::DirectorResourceManager). This mirrors BrnArbStateRaceIntro.cpp,
// which reaches GetEventIntroShots through the same slice.

// ============================================================================
// BrnDirector::ArbStatePostEvent -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct          @0x8225AD60
//   GetName            @0x821F6310
//   PickAppropriateShot@0x8221A080  (declaration-only -- see the FLAG below)
//   Prepare            @0x8226E228
//   Update             @0x82235AA8
//   Release            @0x82235CF0
//   Destruct           (DWARF :308; no asm recovered -- declaration-only)
//
// The director's "post-event" arbitrator state. On Prepare it fetches the event-completion
// shot-group from the resource manager, picks the completion shot that suits the live
// finish-line geometry (PickAppropriateShot), allocates an ICE-anim camera behaviour to
// play it, and configures that behaviour. Update walks the post-event state machine,
// copying the behaviour's produced camera into the state's own camera each frame, forcing
// the live gameplay camera to finish so the post-event take can take over, optionally
// swapping the behaviour's movie (rank-up reveal) and playing the one-shot "Car_Reset"
// flash; once the event leaves the POST_EVENT phase it hands control back to the roaming
// state. All member access is BY NAME; the GameState snapshot it reacts to
// (lrSharedInfo.mpGameState) is reached through the named BrnDirector::GameState members.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The blend the ACTIVE-state "Car_Reset" camera effect plays at (flt_82001C98 == 1.0;
        // shared with BrnArbStateRaceIntro's KF_CAR_RESET_BLEND).
        const f32 KF_CAR_RESET_BLEND = 1.0f;

        // The dirty-flag bit Update raises on the state's camera while a post-event behaviour
        // is driving it (X360 mCamera.mState_uFlags |= 2).
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;

        // The rank-up-reveal gate the ACTIVE state checks before swapping the post-event movie
        // (X360: GameState +0x1AC s32 > 4).
        const s32 KI_RANK_UP_REVEAL_MIN_RANK = 4;
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::GetProducedCamera -- the camera the live ICE-anim behaviour produced
    // this frame (the manager keeps it alongside the behaviour; modelled here BY NAME as the
    // behaviour's produced camera). Defined out-of-line where BehaviourIceAnim is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    const Camera::Camera& ArbStatePostEvent::BehaviourHandle<TBehaviour>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return mpBehaviour->GetProducedCamera();
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::IsBehaviourReadyToUse -- the take is ready once the manager no longer
    // has the behaviour queued for its first Prepare. Defined out-of-line where the
    // BehaviourManager type is complete (same pattern as BrnArbStateRankUp).
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    bool ArbStatePostEvent::BehaviourHandle<TBehaviour>::IsBehaviourReadyToUse() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return !mpManager->IsBehaviourWaitingToPrepare(muAllocationKey);
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225AD60 -- build the camera, clear the base camera flags, and zero the
    // play-flash gate + state machine + behaviour handle.
    // ------------------------------------------------------------------------
    void ArbStatePostEvent::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        mbPlayedFlash = false;             // +0x184 = 0

        meState = E_STATE_INACTIVE;        // +0x188 = 0

        // The behaviour handle starts unallocated (+0x18C block zeroed: mbAllocated,
        // muAllocationKey, muHelperIndex, mpManager, mpBehaviour).
        mPostEventCam = BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6310
    // ------------------------------------------------------------------------
    const char* ArbStatePostEvent::GetName() const
    {
        return "ArbStatePostEvent";
    }

    // ------------------------------------------------------------------------
    // Destruct (DWARF BrnArbStatePostEvent.h:308) -- DECLARATION-ONLY.
    //
    // The DWARF declares a virtual Destruct override for this state, but no asm body was
    // recovered (it is not in the ledger's 6-function postmortem set). FLAG: the body is not
    // reconstructed -- fabricating one would be a guess. The override is declared in the
    // header to keep the X360 vtable slot; the body is left to the unrecovered TU / the base
    // (the per-TU cl /c gate does not link it). No definition is emitted here.
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // PickAppropriateShot @0x8221A080 -- DECLARATION-ONLY.
    //
    // The X360 body looks up the "ShotList" array attribute on the completion shot-group,
    // takes its Length (the number of shots), and switches on it (1 / 2 / 3 / 5):
    //   * 1 shot          -> index 0.
    //   * 2 shots         -> a finish-line flag (mpGameState +0x104) selects index 0 or 1.
    //   * 3 shots         -> when the flag is clear, index 2; otherwise a finish-line-vs-
    //                        player-car dot product (vmsum3fp128 of mFinishLineNorthmostDir
    //                        against a player-car vector lane) selects index 0 or 1.
    //   * 5 shots         -> when the flag is clear, index 4; otherwise Camera::Utils::
    //                        CreateLookAt builds a basis from the finish-line dir and TWO
    //                        vmsum3fp128 dot products against the player-car lane select among
    //                        indices {0, 1, 2, 3}.
    //   * any other count -> assert "Invalid number of shots in group" then index 0.
    // It then returns Attrib::Instance::GetAttributePointer for the chosen index (falling back
    // to Attrib::DefaultDataArea(0x18) when null).
    //
    // FLAG: the shot-selection for the 3- and 5-shot cases is a MULTI-STAGE VMX PIPELINE
    // (vmsum3fp128 dot products + CreateLookAt basis build + vcmpgtfp selects) over vector
    // lanes whose per-field roles in the GameState / player-car objects are NOT recovered.
    // The project rules forbid paraphrasing a multi-stage VMX pipeline to a scalar / inventing
    // a per-axis formula, so this function is left declaration-only rather than fabricated.
    // Prepare names the call; the per-TU cl /c gate does not link the body. Regrow with a
    // faithful VMX reconstruction once the player-car / finish-line vector lanes are pinned.
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Prepare @0x8226E228 -- enter the post-event state: fetch the event-completion shot-group,
    // pick the completion shot, allocate and configure the ICE-anim behaviour. Only runs the
    // setup when not already ACTIVE / CHANGING_TO_ROAMING and the behaviour is not already
    // allocated. Returns true once the behaviour is ready (handle no longer waiting).
    // ------------------------------------------------------------------------
    bool ArbStatePostEvent::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already running (ACTIVE / CHANGING_TO_ROAMING): do nothing, report ready. (X360:
        // meState == 2 || meState == 3 -> return 1.)
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
        {
            return true;
        }

        if (!mPostEventCam.IsAllocated())   // +0x18C block
        {
            GameState& lrGameState = *lrSharedInfo.mpGameState;

            // ---- resolve the event-completion shot-group -------------------------------
            // X360: GetEventCompletionShots(resourceManager, mpGameState->meEventType (+0x120),
            //                               mpGameState->mFinishLineID (+0x160)).
            const Attrib::Gen::shotgroup& lrEventCompleteShotGroup =
                lrSharedInfo.mpDirectorResourceManager->GetEventCompletionShots(
                    lrGameState.meEventType,
                    static_cast<s64>(lrGameState.mFinishLineID));

            CGS_ASSERT(lrEventCompleteShotGroup.Num_ShotList() > 0,
                       "lEventCompleteShotGroup.Num_ShotList()>0");

            Attrib::Gen::iceanim& lrShot =
                PickAppropriateShot(lrEventCompleteShotGroup, lrSharedInfo);

            // ---- allocate + configure the ICE-anim behaviour --------------------------
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mPostEventCam, this, 0, 1);

            mPostEventCam.GetBehaviour()->SetParameters(&lrShot);

            // Seed the bystander vehicle ref the post-event take frames the winner against
            // (X360 stores into mBystanderRef @+0xE10: word 0, word -1, word 0, byte 1).
            mPostEventCam.GetBehaviour()->SetBystanderRefForPostEvent();

            mPostEventCam.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
            mPostEventCam.GetBehaviour()->ClearBaseFirstFrameGate();     // base +0x28 = 0

            meState = E_STATE_PREPARING;   // +0x188 = 1
        }

        // Ready once the freshly-allocated behaviour is no longer queued for its first
        // Prepare (X360: return !IsBehaviourWaitingToPrepare(this+0x18C)).
        return mPostEventCam.IsBehaviourReadyToUse();
    }

    // ------------------------------------------------------------------------
    // Update @0x82235AA8 -- per-frame post-event state machine.
    // ------------------------------------------------------------------------
    void ArbStatePostEvent::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera    = GetNonConstCamera();
        GameState&      lrGameState = *lrSharedInfo.mpGameState;

        switch (meState)
        {
        case E_STATE_INACTIVE:
            break;

        case E_STATE_PREPARING:
            // Run Prepare; on success advance to ACTIVE, clear the flash gate, and force the
            // live gameplay camera to finish so the post-event camera can take over.
            if (Prepare(lrSharedInfo))
            {
                meState       = E_STATE_ACTIVE;   // +0x188 = 2
                mbPlayedFlash = false;            // +0x184 = 0
                lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
                // fall through into the ACTIVE work (X360 goto LABEL_4).
            }
            else
            {
                break;
            }
            // FALLTHROUGH
        case E_STATE_ACTIVE:
        {
            // Drive the state camera from the behaviour and mark it behaviour-driven.
            lrCamera = mPostEventCam.GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;

            // Rank-up reveal: when a rank-up message was received this frame and the new rank
            // is past 4, swap the post-event take to the rank-up completion movie. (X360:
            // gameState +0x1AB byte && gameState +0x1AC s32 > 4.) FLAG: the +0x1AB/+0x1AC field
            // identities (mbRankUpMessageReceivedThisFrame / miRankUpNewRank) are inferred from
            // the DecFIGS DWARF member order for that named region, not offset-pinned; the
            // VALUE test (byte != 0 && s32 > 4) is asm-attested.
            if (lrGameState.mbRankUpMessageReceivedThisFrame &&
                lrGameState.miRankUpNewRank > KI_RANK_UP_REVEAL_MIN_RANK)
            {
                const DirectorResourceManager& lrResourceManager =
                    *lrSharedInfo.mpDirectorResourceManager;

                // X360: GetAttributePointer(resourceManager + 0x508, ShotList key, 0), falling
                // back to DefaultDataArea(0x18) when null.
                //
                // ⭐ 2026-07-31: this used to go through a fork accessor called
                // `GetPostEventMovieData()`. There is no such member. Console byte 0x508 == 1288
                // is mBurnoutLicense, whose DWARF accessor is GetBurnoutLicense(), so the site is
                // an INLINED EXPRESSION over that group -- resolve its ShotList element 0 -- that
                // had been mistaken for a single accessor. Spelled out here; the retired
                // DirectorResourceManager fork in BrnBehaviourIceAnim.h records the same finding.
                const void* lpShotData = lrResourceManager.GetBurnoutLicense().GetShotListData(0);
                if (lpShotData == 0)
                    lpShotData = Attrib::DefaultDataArea(0x18u);

                Camera::BehaviourIceAnim::ShotReference* lpMovieBlock =
                    reinterpret_cast<Camera::BehaviourIceAnim::ShotReference*>(
                        const_cast<void*>(lpShotData));

                mPostEventCam.GetBehaviour()->ChangeMovie(lpMovieBlock, lrResourceManager);
            }

            // Once the event leaves the POST_EVENT phase, hand control back to roaming.
            if (lrGameState.GetCurrentEventState() != GameState::E_EVENT_STATE_POST_EVENT)
            {
                if (mbPlayedFlash)
                {
                    // Already flashed: force the gameplay camera to finish and hand back.
                    lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
                    ArbUtils::ChangeToState<EState>(
                        this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                        meState, E_STATE_CHANGING_TO_ROAMING);
                }
                else
                {
                    // First time: play the one-shot "Car_Reset" flash, force the gameplay
                    // camera to finish, and latch the flash gate.
                    Camera::EnsureEffectIsPlaying(lrCamera, *lrSharedInfo.mpEffectInterface,
                                                  "Car_Reset", KF_CAR_RESET_BLEND);
                    lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
                    mbPlayedFlash = true;   // +0x184 = 1
                }
            }
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
            // Drive the state camera from the behaviour one last frame, then hand control back
            // to roaming.
            lrCamera = mPostEventCam.GetProducedCamera();
            lrCamera.mState_uFlags |= KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN;
            ArbUtils::ChangeToState<EState>(
                this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                meState, E_STATE_CHANGING_TO_ROAMING);
            break;

        default:
            CGS_ASSERT(false, "unhandled state");
            break;
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82235CF0 -- leave the post-event state: reset the state machine, release the
    // ICE-anim behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStatePostEvent::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x188 = 0

        if (mPostEventCam.IsAllocated())   // +0x18C block
        {
            mPostEventCam.mpManager->UnSetBehaviourUsedByHandle(mPostEventCam.muAllocationKey);
            mPostEventCam.muHelperIndex = 0;
            mPostEventCam.mpManager     = 0;
            mPostEventCam.mpBehaviour   = 0;
            mPostEventCam.mbAllocated   = false;
        }

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
