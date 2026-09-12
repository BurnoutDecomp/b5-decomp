#include "GameSource/Director/Arbitrator/States/BrnArbStatePostEvent.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (post-event asserts)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::EnsureEffectIsPlaying
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                  // Camera::VehicleInfo (the player car)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                       // Camera::Utils::CreateLookAt
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim + DirectorResourceManager slice
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager (complete)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup

// NOTE: the director-camera cone uses the minimal DirectorResourceManager SLICE declared in
// BrnBehaviourIceAnim.h (GetEventCompletionShots / GetBurnoutLicense), NOT the heavier
// BrnDirectorResourceManager.h home -- including both in one TU would be an ODR conflict (two
// definitions of BrnDirector::DirectorResourceManager). This mirrors BrnArbStateRaceIntro.cpp,
// which reaches GetEventIntroShots through the same slice.

// ============================================================================
// BrnDirector::ArbStatePostEvent -- Construct / GetName / PickAppropriateShot / Prepare /
// Update / Release (plus the empty Destruct slot, flagged at its definition).
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
        // The blend the ACTIVE-state "Car_Reset" camera effect plays at (1.0; shared with
        // BrnArbStateRaceIntro's KF_CAR_RESET_BLEND).
        const f32 KF_CAR_RESET_BLEND = 1.0f;

        // The dirty-flag bit Update raises on the state's camera while a post-event behaviour
        // is driving it (mCamera.mState_uFlags |= 2).
        const s32 KI_CAMERA_DIRTY_BEHAVIOUR_DRIVEN = 2;

        // The rank-up-reveal gate the ACTIVE state checks before swapping the post-event movie
        // (GameState rank-up rank > 4).
        const s32 KI_RANK_UP_REVEAL_MIN_RANK = 4;

        // The three-lane dot product the shot picker reduces with (the shipped build uses the
        // three-lane vector dot; the w lane never takes part).
        inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
        {
            return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z;
        }

        // Magnitude only -- the picker compares the two dots with their sign bits cleared.
        inline f32 Abs(f32 lf) { return lf < 0.0f ? -lf : lf; }
    }

    // ------------------------------------------------------------------------
    // Construct -- build the camera, clear the base camera flags, and zero the play-flash
    // gate + state machine + behaviour handle.
    // ------------------------------------------------------------------------
    void ArbStatePostEvent::Construct()
    {
        GetNonConstCamera().Construct();

        ResetBaseCameraFlags();

        mbPlayedFlash = false;             // +0x184 = 0

        meState = E_STATE_INACTIVE;        // +0x188 = 0

        // The behaviour handle starts unallocated (+0x18C block zeroed).
        mPostEventCam = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName
    // ------------------------------------------------------------------------
    const char* ArbStatePostEvent::GetName() const
    {
        return "ArbStatePostEvent";
    }

    // ------------------------------------------------------------------------
    // Destruct -- FLAG: EMPTY BY ABSENCE, NOT BY RECONSTRUCTION.
    //
    // This state declares a Destruct() override (it owns that vtable slot), but the console
    // image carries no separate body for it and no call site: the whole recovered function
    // set for this state is Construct / GetName / PickAppropriateShot / Prepare / Update /
    // Release. The empty body below exists so the slot resolves; it is NOT recovered code,
    // and nothing here is invented state. Regrow it if a body is ever recovered.
    // ------------------------------------------------------------------------
    void ArbStatePostEvent::Destruct()
    {
    }

    // ------------------------------------------------------------------------
    // PickAppropriateShot -- choose which of the event-completion group's shots the post-event
    // take plays, then return that ShotList element.
    //
    // The recovered body reads the group's ShotList length and switches on it. Every arm is a
    // pure selection of an element INDEX; the tail is the same indexed element resolve every
    // other shot consumer uses (the attribute pointer for that index, with the 24-byte default
    // data area as the null-element fallback -- which is exactly shotgroup::GetShotListElement).
    //
    //   1 shot  -> index 0.
    //   2 shots -> the "player won the last event" flag picks 0 (won) or 1 (lost).
    //   3 shots -> lost: index 2. Won: the sign of dot3(finish-line northmost direction,
    //              player car linear velocity) picks 0 (heading north along the line's
    //              direction) or 1.
    //   5 shots -> lost: index 4. Won: build a look-at frame from the origin down the
    //              finish-line direction and take its x axis NEGATED (the "across" axis), then
    //              compare |dot3(dir, velocity)| against |dot3(across, velocity)| to decide
    //              whether the car is running mostly ALONG the line or mostly ACROSS it, and
    //              take the sign of the winning dot for the side:
    //                  along  wins -> +ve 0, -ve 2
    //                  across wins -> +ve 1, -ve 3
    //   anything else -> assert, then index 0.
    //
    // FLAG (values, not roles): the recovered assert text names the group id and the finish
    // line, so the three-shot/five-shot arms are finish-line geometry by the code's own words;
    // the member identities used here (GameState::mbWonLastEvent, mFinishLineNorthmostDir and
    // the player car's mRaceCarState.mLinearVelocity) are pinned by offset from this build's
    // own GameState / shared-info / race-car-state layouts, all three already committed.
    // ------------------------------------------------------------------------
    Camera::Camera::ShotReference& ArbStatePostEvent::PickAppropriateShot(
        const Attrib::Gen::shotgroup& lrShotGroup, ArbStateSharedInfo& lrSharedInfo)
    {
        const GameState& lrGameState = *lrSharedInfo.mpGameState;

        u32 luShotIndex = 0;

        switch (lrShotGroup.Num_ShotList())
        {
        case 1u:
            luShotIndex = 0u;
            break;

        case 2u:
            luShotIndex = lrGameState.mbWonLastEvent ? 0u : 1u;
            break;

        case 3u:
            if (!lrGameState.mbWonLastEvent)
            {
                luShotIndex = 2u;
            }
            else
            {
                const f32 lfAlong = Dot3(lrGameState.mFinishLineNorthmostDir,
                                         lrSharedInfo.mpPlayerCar->mRaceCarState.mLinearVelocity);
                luShotIndex = (lfAlong > 0.0f) ? 0u : 1u;
            }
            break;

        case 5u:
            if (!lrGameState.mbWonLastEvent)
            {
                luShotIndex = 4u;
            }
            else
            {
                const Vector3& lrFinishLineDir = lrGameState.mFinishLineNorthmostDir;
                const Vector3& lrVelocity =
                    lrSharedInfo.mpPlayerCar->mRaceCarState.mLinearVelocity;

                // The look-at frame built from the origin along the finish-line direction; its
                // x axis is the line's "right", and the across axis the picker dots against is
                // that axis NEGATED -- taken here as the negated dot, which is the same value.
                Vector3 lOrigin;
                lOrigin.SetZero();
                const Matrix44Affine lFinishLineFrame =
                    Camera::Utils::CreateLookAt(lOrigin, lrFinishLineDir);

                const f32 lfAlong  = Dot3(lrFinishLineDir, lrVelocity);
                const f32 lfAcross = -(lFinishLineFrame.xAxis.x * lrVelocity.x +
                                       lFinishLineFrame.xAxis.y * lrVelocity.y +
                                       lFinishLineFrame.xAxis.z * lrVelocity.z);

                if (Abs(lfAlong) > Abs(lfAcross))
                    luShotIndex = (lfAlong > 0.0f) ? 0u : 2u;
                else
                    luShotIndex = (lfAcross > 0.0f) ? 1u : 3u;
            }
            break;

        default:
            CGS_ASSERT(false, "Invalid number of shots in group");
            luShotIndex = 0u;
            break;
        }

        // The indexed element resolve with the 24-byte default-data-area null fallback.
        return *static_cast<Camera::Camera::ShotReference*>(
            const_cast<void*>(lrShotGroup.GetShotListElement(luShotIndex)));
    }

    // ------------------------------------------------------------------------
    // Prepare -- enter the post-event state: fetch the event-completion shot-group, pick the
    // completion shot, allocate and configure the ICE-anim behaviour. Only runs the setup when
    // not already ACTIVE / CHANGING_TO_ROAMING and the behaviour is not already allocated.
    // Returns true once the behaviour is ready (handle no longer waiting to prepare).
    // ------------------------------------------------------------------------
    bool ArbStatePostEvent::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Already running (ACTIVE / CHANGING_TO_ROAMING): do nothing, report ready.
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
        {
            return true;
        }

        if (!mPostEventCam.IsAllocated())   // +0x18C block
        {
            GameState& lrGameState = *lrSharedInfo.mpGameState;

            // ---- resolve the event-completion shot-group -------------------------------
            const Attrib::Gen::shotgroup& lrEventCompleteShotGroup =
                lrSharedInfo.mpDirectorResourceManager->GetEventCompletionShots(
                    lrGameState.meEventType,
                    static_cast<s64>(lrGameState.mFinishLineID));

            CGS_ASSERT(lrEventCompleteShotGroup.Num_ShotList() > 0,
                       "lEventCompleteShotGroup.Num_ShotList()>0");

            Camera::BehaviourIceAnim::ShotReference& lrShot =
                PickAppropriateShot(lrEventCompleteShotGroup, lrSharedInfo);

            // ---- allocate + configure the ICE-anim behaviour --------------------------
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mPostEventCam, this, 0, 1);

            mPostEventCam.GetBehaviour()->SetParameters(&lrShot);

            // Seed the bystander vehicle ref the post-event take frames the winner against.
            mPostEventCam.GetBehaviour()->SetBystanderRefForPostEvent();

            mPostEventCam.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28
            mPostEventCam.GetBehaviour()->ClearBaseFirstFrameGate();     // base +0x28 = 0

            meState = E_STATE_PREPARING;   // +0x188 = 1
        }

        // Ready once the freshly-allocated behaviour is no longer queued for its first Prepare.
        return !mPostEventCam.IsWaitingToPrepare();
    }

    // ------------------------------------------------------------------------
    // Update -- per-frame post-event state machine.
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
                // fall through into the ACTIVE work.
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
            // is past 4, swap the post-event take to the rank-up completion movie. FLAG: the
            // two field identities (mbRankUpMessageReceivedThisFrame / miRankUpNewRank) are
            // inferred from the member order for that named region, not offset-pinned; the
            // VALUE test (byte != 0 && s32 > 4) is attested.
            if (lrGameState.mbRankUpMessageReceivedThisFrame &&
                lrGameState.miRankUpNewRank > KI_RANK_UP_REVEAL_MIN_RANK)
            {
                const DirectorResourceManager& lrResourceManager =
                    *lrSharedInfo.mpDirectorResourceManager;

                // Resource-manager byte 1288 is mBurnoutLicense, whose accessor is
                // GetBurnoutLicense(); the site is an INLINED EXPRESSION over that group --
                // resolve its ShotList element 0 -- that had once been mistaken for a single
                // "GetPostEventMovieData" accessor. Spelled out here; the retired
                // DirectorResourceManager fork in BrnBehaviourIceAnim.h records the same
                // finding.
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
    // Release -- leave the post-event state: reset the state machine, release the ICE-anim
    // behaviour back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStatePostEvent::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // +0x188 = 0

        // The handle release the console inlines here is the shared handle's own Release():
        // when allocated, UnSetBehaviourUsedByHandle(muAllocationKey) on the owning manager,
        // then zero the five handle words.
        mPostEventCam.Release();

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
