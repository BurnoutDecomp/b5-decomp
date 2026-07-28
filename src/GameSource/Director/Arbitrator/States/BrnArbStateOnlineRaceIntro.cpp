#include "GameSource/Director/Arbitrator/States/BrnArbStateOnlineRaceIntro.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT (online-race-intro asserts)
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"          // ArbUtils::ChangeToState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::EState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // BrnDirector::GameState
#include "GameSource/Director/Camera/Camera.h"                                  // BrnDirector::Camera::Camera (mState_uFlags)
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer (gameplay cam / force-finish)
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // Camera::StopCurrentEffect / EnsureEffectIsStopped
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // Camera::BehaviourIceAnim + DirectorResourceManager slice
#include "GameSource/Director/Utils/BrnICEMoviePlayer.h"                        // Camera::BehaviourManager + Camera::BehaviourInterpolate (slice)
#include "GameSource/AttribSys/Generated/classes/shotgroup.h"                   // Attrib::Gen::shotgroup + Attrib::DefaultDataArea

// ============================================================================
// BrnDirector::ArbStateOnlineRaceIntro -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   CalculateStateTimes  @0x821F6370
//   GetName              @0x821F6480
//   Construct            @0x8225AE98
//   SetupRivalMovie      @0x8226EC18
//   Update               @0x822734A8
//   (Prepare body @0x821F6340 -- the small "latch meState" entry, mirrored from the DWARF +
//    the case-1 dispatch; Release/Destruct are declaration-only -- no attested asm in this TU.)
//
// The director's ONLINE race-intro arbitrator state. It plays a pre-race camera fly-by: it
// shows each rival in turn (the three rival "show" ICE-anim takes, cycled through
// muRivalMovieOffset), blends between them with a camera interpolator, then shows the player and
// the start lights, before the countdown hands control back to the roaming state. CalculateState
// Times splits the event's intro time budget across the per-rival show/move segments. Update
// walks the state machine, each frame copying whichever behaviour's produced camera is in force
// into the state's own camera and driving the "BlackFadeIn_Quick" fade; once the event leaves
// the intro/countdown it changes to roaming.
//
// All member access is BY NAME. The GameState snapshot it reacts to (lrSharedInfo.mpGameState)
// is reached through the named BrnDirector::GameState members.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // The default attrib data-area size requested when a shot's parameter block is absent
        // (X360 li r3, 0x18 -> Attrib::DefaultDataArea(0x18)). Same as the sibling states.
        const u32 KU_SHOT_DEFAULT_DATA_AREA_SIZE = 0x18u;

        // The two trailing arguments the BehaviourManager::NewBehaviour<TBehaviour> allocation
        // request carries (X360 li r6,0 / li r7,1). RETYPED 2026-07-29: r6 is NewBehaviour's
        // OWNER slot -- a `const void*` (the arbitrator states pass null there; the moments pass
        // their Moment*) -- and r7 is the s32 reference LIMIT. Declaring the owner as `const s32`
        // meant the call matched no overload at all: a `const s32` variable is not an integer
        // LITERAL, so it is not a null-pointer constant and will not convert to `const void*`.
        // (That is the Step-0 defect the previous wave recorded as "one of the two call sites'
        // arg lists is wrong" -- it was the TYPE, not the count.)
        const void* const KPC_NEW_BEHAVIOUR_OWNER   = 0;
        const s32         KI_NEW_BEHAVIOUR_REFLIMIT = 1;

        // CalculateStateTimes constants (asm-attested rodata: flt_82003F40 / flt_82001CC0 /
        // flt_82001D9C / flt_82001C98).
        const f32 KF_FIXED_TIME_DEDUCTION = 0.25f;   // flt_82003F40: subtracted from lfMaxTime
        const f32 KF_ZERO                 = 0.0f;    // flt_82001CC0: the >0 assert compare / resets
        const f32 KF_MOVE_SEGMENTS_SCALE  = 2.0f;    // flt_82001D9C: per-rival move-segment weight
        const f32 KF_FULL_SHOW_TIME       = 1.0f;    // flt_82001C98: the clamped per-show time cap

        // The hook the intro epilogue keeps in force / stops on the state camera.
        const char* const KPC_HOOK_BLACK_FADE_IN_QUICK = "BlackFadeIn_Quick";

        // The minimum number of shots the online-race-start group must carry (the case-1 assert
        // "lOnlineRaceStartShotGroup.Num_ShotList() >= E_ICE_MOVIE_MIN_COUNT"). X360 cmpwi 3.
        const u32 KU_ICE_MOVIE_MIN_COUNT = 3;

        // muNumberOfCarsInIntro must be in [1, 3] (the case-1 assert).
        const u32 KU_MAX_CARS_IN_INTRO = 3;

        // The behaviour-internal "give up following" sentinel the X360 writes onto the forced-
        // finished gameplay behaviour (+0x290 == 0x7F7FFFFF == FLT_MAX bits); reproduced via the
        // named SharedCameraContainer operation, not poked by offset.
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::GetProducedCamera -- the camera the live behaviour produced this frame
    // (the manager keeps it alongside the behaviour; the X360 reads slot+0x10 via sub_821FD450).
    // Defined out-of-line where the behaviour types are complete. Both the ICE-anim and the
    // interpolate behaviour expose the produced camera by name.
    // ------------------------------------------------------------------------
    template <>
    const Camera::Camera&
    ArbStateOnlineRaceIntro::BehaviourHandle<Camera::BehaviourIceAnim>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        return mpBehaviour->GetProducedCamera();
    }

    template <>
    const Camera::Camera&
    ArbStateOnlineRaceIntro::BehaviourHandle<Camera::BehaviourInterpolate>::GetProducedCamera() const
    {
        CGS_ASSERT(mbAllocated, "IsAllocated()");
        // The interpolator's produced camera lives at the same manager-pool slot offset the
        // ICE-anim behaviour's does (the X360 sub_821FCD40 reads slot+0x10); reached BY NAME
        // through the interpolator's GetCamera() accessor.
        // FLAG: the BehaviourInterpolate produced-camera accessor is the minimal slice -- the
        // camera ROLE is reproduced (sub_821FCD40 reads slot+0x10).
        return mpBehaviour->GetCamera();
    }

    // ------------------------------------------------------------------------
    // BehaviourHandle::Release -- drop the manager-side hold on the behaviour and clear the
    // handle (X360 sub_8222DFD8 on the ICE-anim handles / sub_8222DCA8 on the interpolator).
    // Defined out-of-line where the BehaviourManager type is complete.
    // ------------------------------------------------------------------------
    template <typename TBehaviour>
    bool ArbStateOnlineRaceIntro::BehaviourHandle<TBehaviour>::Release()
    {
        if (mbAllocated)
        {
            mpManager->UnSetBehaviourUsedByHandle(muAllocationKey);
            muHelperIndex = 0;
            mpManager     = 0;
            mpBehaviour   = 0;
            mbAllocated   = false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6480
    // ------------------------------------------------------------------------
    const char* ArbStateOnlineRaceIntro::GetName() const
    {
        return "ArbStateOnlineRaceIntro";
    }

    // ------------------------------------------------------------------------
    // Construct @0x8225AE98 -- build the camera, clear the base camera flags, zero the state
    // machine + the rival-movie cursor, and zero every behaviour handle + the interpolator
    // parameter block.
    // ------------------------------------------------------------------------
    void ArbStateOnlineRaceIntro::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // X360 stb 0, +0x170 / +0x171

        meState            = E_STATE_INACTIVE;   // +0x220 = 0
        muRivalMovieOffset = 0;                  // +0x21C = 0

        // The interpolator handle starts unallocated (+0x180 block zeroed).
        mInterpolator = BehaviourHandle<Camera::BehaviourInterpolate>();

        // The interpolator parameter block seed (+0x194: {+0x00:0, +0x04:8, +0x08:0, +0x0C:2}).
        mInterpolatorParams           = InterpolatorParameters();
        mInterpolatorParams.muField04 = 8;
        mInterpolatorParams.muField0C = 2;

        // Every ICE-anim handle starts unallocated (rival[0..2] @+0x1A4/+0x1B8/+0x1CC,
        // mPlayer @+0x1E0, mLights @+0x1F4 blocks zeroed).
        for (u32 luRival = 0; luRival < KU_NUM_RIVAL_BEHAVIOURS; ++luRival)
        {
            maRivalBehaviourHandle[luRival] = BehaviourHandle<Camera::BehaviourIceAnim>();
        }
        mPlayerBehaviourHandle = BehaviourHandle<Camera::BehaviourIceAnim>();
        mLightsBehaviourHandle = BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // Prepare @0x821F6340 -- the small entry latch: unless already ACTIVE-or-later, force
    // PREPARING and reset the rival-movie cursor, then report ready (the X360 returns 1).
    //
    //   if (meState <= 1) { meState = 1; muRivalMovieOffset = 0; }   return true;
    //
    // (X360: `if (*(a1+544) <= 1u) { *(a1+544)=1; *(a1+536)=0; }` -- +0x220 == meState,
    // +0x21C == muRivalMovieOffset on the console; the DWARF/asm word offsets quoted in the
    // pseudocode packet are the console layout, reached here BY NAME.)
    // ------------------------------------------------------------------------
    bool ArbStateOnlineRaceIntro::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        (void)lrSharedInfo;   // the entry latch does not consult the shared info

        if (meState <= E_STATE_PREPARING)
        {
            meState        = E_STATE_PREPARING;   // +0x220 = 1 (stw r10)
            muCurrentRival = 0;                   // +0x218 = 0 (stw r9) -- asm zeroes the rival cursor, not +0x21C
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // CalculateStateTimes @0x821F6370 -- split the event's intro time budget across the per-rival
    // "show" / "move" segments.
    //
    // luNumRivals == 0: put the whole (un-deducted) budget in the player "show" time and zero the
    // two per-rival times. Otherwise: deduct a fixed 0.25s, assert the remainder is > 0, then
    // weight the budget by (numRivals+1) shows plus 2*(numRivals+1) move segments. When the
    // budget covers the full weighted time, every show gets budget/totalWeight and every move
    // twice that; when it is short, every show is capped at 1.0 and the moves absorb the
    // remainder. (The X360 int->double conversion of numRivals / numRivals+1 is reproduced as
    // plain f32 arithmetic; the four rodata constants are asm-attested.)
    // ------------------------------------------------------------------------
    void ArbStateOnlineRaceIntro::CalculateStateTimes(u32 luNumRivals, f32 lfMaxTime)
    {
        if (luNumRivals != 0)
        {
            const f32 lfBudget = lfMaxTime - KF_FIXED_TIME_DEDUCTION;
            CGS_ASSERT(lfBudget > KF_ZERO, "lfMaxTime > 0.0f");

            // Total weighted time: (numRivals+1) shows + 2*(numRivals+1) moves.
            const f32 lfShowCount      = static_cast<f32>(luNumRivals + 1);
            const f32 lfMoveWeight     = static_cast<f32>(luNumRivals) * KF_MOVE_SEGMENTS_SCALE
                                         + KF_MOVE_SEGMENTS_SCALE;
            const f32 lfTotalWeight    = lfShowCount + lfMoveWeight;

            // asm @0x821F6370: fcmpu cr6, totalWeight, budget; bge -> the budget-covers branch.
            if (lfTotalWeight >= lfBudget)
            {
                // Budget covers the full fly-by: interpolate gets an even slice, each look-segment 2x.
                mfTimeToSpendInterpolating   = lfBudget / lfTotalWeight;            // +0x208
                mfTimeToSpendLookingAtRival  = (lfBudget / lfTotalWeight) * KF_MOVE_SEGMENTS_SCALE; // +0x20C
                mfTimeToSpendLookingAtPlayer = mfTimeToSpendLookingAtRival;         // +0x210
            }
            else
            {
                // Budget is short: cap interpolation at the full time (1.0), give the look-segments the rest.
                mfTimeToSpendInterpolating   = KF_FULL_SHOW_TIME;                   // +0x208 = 1.0
                mfTimeToSpendLookingAtRival  =
                    (KF_MOVE_SEGMENTS_SCALE / lfMoveWeight) * (lfBudget - lfTotalWeight)
                    + KF_MOVE_SEGMENTS_SCALE;                                       // +0x20C
                mfTimeToSpendLookingAtPlayer = mfTimeToSpendLookingAtRival;         // +0x210
            }
        }
        else
        {
            mfTimeToSpendLookingAtPlayer = lfMaxTime;
            mfTimeToSpendLookingAtRival  = KF_ZERO;
            mfTimeToSpendInterpolating   = KF_ZERO;
        }
    }

    // ------------------------------------------------------------------------
    // SetupRivalMovie @0x8226EC18 -- allocate + configure the rival "show" ICE-anim behaviour at
    // the current rival-movie cursor (muRivalMovieOffset) for rival luRivalIndex.
    //
    // Picks the rival's shot from the online-race-start shot group (indexing
    // muCurrentRival % luNumRivalMovies into the ShotList, after the two leading non-rival
    // shots), feeds its data block to a fresh ICE-anim behaviour, and configures the behaviour's
    // primary/secondary anchor vehicle references to the rival's tracked vehicle. Advances the
    // cursor afterward.
    // ------------------------------------------------------------------------
    void ArbStateOnlineRaceIntro::SetupRivalMovie(ArbStateSharedInfo& lrSharedInfo, u32 luRivalIndex)
    {
        const Attrib::Gen::shotgroup& lrOnlineRaceStartShotGroup =
            lrSharedInfo.mpDirectorResourceManager->GetOnlineRaceStartShots();

        // The number of RIVAL movies is the ShotList length minus the two leading non-rival
        // shots (the player + lights takes).
        const u32 luNumRivalMovies = lrOnlineRaceStartShotGroup.Num_ShotList() - 2;
        CGS_ASSERT(luNumRivalMovies > 0, "luNumRivalMovies > 0");

        // The shot index for this rival: wrap the rival counter into the rival range, then skip
        // the two leading non-rival shots.
        const u32 luShotIndex = muCurrentRival % luNumRivalMovies + 2;

        // Allocate the rival "show" behaviour into the handle at the current cursor.
        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
            maRivalBehaviourHandle[muRivalMovieOffset], this,
            KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

        maRivalBehaviourHandle[muRivalMovieOffset].GetBehaviour()
            ->SetForceMotionBlurEverything(true);   // +0xE2B = 1

        const void* lpShotData = const_cast<Attrib::Gen::shotgroup&>(lrOnlineRaceStartShotGroup)
                                     .GetShotListData(static_cast<s32>(luShotIndex));
        if (!lpShotData)
            lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

        Camera::BehaviourIceAnim* lpBehaviour = maRivalBehaviourHandle[muRivalMovieOffset].GetBehaviour();
        lpBehaviour->SetParameters(
            static_cast<Camera::BehaviourIceAnim::ShotReference*>(const_cast<void*>(lpShotData)));

        // Anchor the take's eye + look vehicle references to this rival's intro race car (the
        // X360 reads the rival's race-car index from the GameState's per-intro-car id table and
        // writes the same race-car ref into both mPrimaryVehicleRef @+0xDF0 and
        // mSecondaryVehicleRef @+0xE00, asserting the index < ku8MaxNumRaceCars inside each
        // setter).
        const s32 liRivalRaceCar =
            static_cast<s32>(lrSharedInfo.mpGameState->maeIntroCarID[luRivalIndex]);

        lpBehaviour->SetPrimaryVehicleRefToRaceCarIndex(liRivalRaceCar);     // +0xDF0 block
        maRivalBehaviourHandle[muRivalMovieOffset].GetBehaviour()
            ->SetSecondaryVehicleRefToRaceCarIndex(liRivalRaceCar);          // +0xE00 block

        // Enable the take's collision policy (X360 stb 1 at +0xE28).
        maRivalBehaviourHandle[muRivalMovieOffset].GetBehaviour()->SetUseCollisionPolicy(true);

        // Clear the base first-frame gate (X360 *(behaviour + 40) = 0).
        maRivalBehaviourHandle[muRivalMovieOffset].GetBehaviour()->ClearBaseFirstFrameGate();

        ++muRivalMovieOffset;
    }

    // ------------------------------------------------------------------------
    // SetupInterpolator -- allocate the take-to-take blend behaviour and latch it to blend
    // lrFromCamera -> lrToCamera over mfTimeToSpendInterpolating, seeded from mInterpolatorParams.
    // De-inlines the X360 interpolator-setup sequence the MOVING_TO_* edges share.
    // ------------------------------------------------------------------------
    void ArbStateOnlineRaceIntro::SetupInterpolator(ArbStateSharedInfo& lrSharedInfo,
                                                    const Camera::Camera& lrFromCamera,
                                                    const Camera::Camera& lrToCamera)
    {
        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourInterpolate>(
            mInterpolator, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

        Camera::BehaviourInterpolate* lpInterpolator = mInterpolator.GetBehaviour();

        // Seed the interpolation mode + the per-take parameters (the X360 writes the method word
        // at behaviour +0x10 from mInterpolatorParams and points the params pointer at the state's
        // InterpolatorParameters block). The InterpolatorParameters POD stands in for the slice's
        // opaque Parameters here -- cast to the slice type the named setter takes.
        lpInterpolator->SetInterpolationMode(static_cast<s32>(mInterpolatorParams.muField0C));
        lpInterpolator->SetParameters(
            reinterpret_cast<const Camera::BehaviourInterpolate::Parameters*>(&mInterpolatorParams));

        lpInterpolator->SetupCameraAFromCamera(lrFromCamera);   // X360 sub_821FD4B8 (from-ref)
        lpInterpolator->SetupCameraBFromCamera(lrToCamera);     // X360 sub_821FD4B8 (to-ref)
        lpInterpolator->SetupDuration(mfTimeToSpendInterpolating);
        lpInterpolator->Setup();                                // X360 sub_8224EE58 latch
    }

    // ------------------------------------------------------------------------
    // Update @0x822734A8 -- per-frame online-race-intro state machine.
    //
    // FLAG (scope of faithful reconstruction): the per-state camera drive, the GameState event-
    // state reads, the ChangeToState hand-offs, the behaviour allocation + ICE-anim configuration
    // and the epilogue fade are reproduced BY NAME from the X360 pseudocode/asm. The take-to-take
    // interpolator SETUP (cases MOVING_TO_RIVAL / MOVING_TO_PLAYER) goes through an unrecovered
    // multi-stage camera-reference pipeline (X360 sub_821FD4B8 builds a CameraReference from a
    // BehaviourHandle, sub_8224EE58 latches the interpolator over the two refs); those steps are
    // expressed through the BehaviourInterpolate named-setup API (declaration-only) rather than
    // paraphrased to per-field stores. The camera-state dirty-flag word writes the X360 ORs into
    // mCamera.mState_uFlags each frame are reproduced as the named flag OR.
    // ------------------------------------------------------------------------
    void ArbStateOnlineRaceIntro::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        Camera::Camera& lrCamera    = GetNonConstCamera();
        GameState&      lrGameState = *lrSharedInfo.mpGameState;

        // The X360 clears the camera's "behaviour-driven" dirty bit at entry
        // (mCamera.mState_uFlags &= ~2).
        lrCamera.mState_uFlags &= ~2;

        if (meState == E_STATE_INACTIVE)
        {
            return;   // case 0 jumps straight to the epilogue-skip (LABEL_79)
        }

        switch (meState)
        {
        case E_STATE_PREPARING:
        {
            // Run Prepare; on success and once the event reaches the INTRO event-state, stop any
            // live effect, split the time budget across the cars in the intro, allocate + config
            // the player "show" behaviour, and advance to ACTIVE (or to WAITING_FOR_COUNTDOWN
            // when there are no cars to show).
            if (Prepare(lrSharedInfo) &&
                lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_INTRO)
            {
                Camera::StopCurrentEffect(lrCamera, *lrSharedInfo.mpEffectInterface);

                const u32 luCarsInIntro = lrGameState.muNumberOfCarsInIntro;
                if (luCarsInIntro != 0)
                {
                    CGS_ASSERT(luCarsInIntro >= 1 && luCarsInIntro <= KU_MAX_CARS_IN_INTRO,
                               "lSharedInfo.mpGameState->muNumberOfCarsInIntro >= 1 && "
                               "lSharedInfo.mpGameState->muNumberOfCarsInIntro <= 3");

                    CalculateStateTimes(luCarsInIntro, lrGameState.mfStateTimeLeft);

                    if (!mPlayerBehaviourHandle.IsAllocated())
                    {
                        const Attrib::Gen::shotgroup& lrOnlineRaceStartShotGroup =
                            lrSharedInfo.mpDirectorResourceManager->GetOnlineRaceStartShots();
                        CGS_ASSERT(lrOnlineRaceStartShotGroup.Num_ShotList() >= KU_ICE_MOVIE_MIN_COUNT,
                                   "lOnlineRaceStartShotGroup.Num_ShotList() >= E_ICE_MOVIE_MIN_COUNT");

                        // Force the primary gameplay behaviour to finish so the intro camera can
                        // take over (X360 resolves the primary gameplay behaviour and sets its
                        // remaining-time to FLT_MAX + raises its finished flags).
                        lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();

                        // Allocate + configure the player "show" behaviour (shot index 1).
                        lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                            mPlayerBehaviourHandle, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

                        mPlayerBehaviourHandle.GetBehaviour()->SetUseCollisionPolicy(true);   // +0xE28 = 1
                        mPlayerBehaviourHandle.GetBehaviour()->ClearBaseFirstFrameGate();     // +0x28 = 0

                        const void* lpShotData =
                            const_cast<Attrib::Gen::shotgroup&>(lrOnlineRaceStartShotGroup)
                                .GetShotListData(/*liShotIndex*/ 1);
                        if (!lpShotData)
                            lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

                        Camera::BehaviourIceAnim* lpBehaviour = mPlayerBehaviourHandle.GetBehaviour();
                        lpBehaviour->SetParameters(
                            static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                                const_cast<void*>(lpShotData)));

                        // Anchor the player take's eye + look references to the player (X360
                        // writes {kind 0, index -1, 0, valid 1} into both refs).
                        lpBehaviour->SetPrimaryVehicleRefToPlayer();     // +0xDF0
                        lpBehaviour->SetSecondaryVehicleRefToPlayer();   // +0xE00

                        mPlayerBehaviourHandle.GetBehaviour()->SetForceMotionBlurEverything(true); // +0xE2B = 1

                        // The trailing reset byte the X360 derives via cntlzw (maReset0DE4[3]).
                        // FLAG: the cntlzw-derived value is a 0/1 boolean of an un-recovered
                        // condition; reproduced as a named byte store of the attested 0/1 result.
                        mPlayerBehaviourHandle.GetBehaviour()->SetTakeResetByte3(1);   // +0xDE7

                        // Mark the state camera behaviour-driven (mState_uFlags |= 0x40).
                        lrCamera.mState_uFlags |= 0x40;
                    }

                    mfTimeInState = 0.0f;        // +0x214 = 0
                    meState       = E_STATE_ACTIVE;   // +0x220 = 2
                }
                else
                {
                    meState = E_STATE_WAITING_FOR_COUNTDOWN;   // +0x220 = 8
                }
            }
            break;
        }

        case E_STATE_ACTIVE:
        {
            // Rewind the player "show" controller to its take start, then mark the camera
            // behaviour-driven (mState_uFlags |= 0x40 and |= 0x10000000), reset the timers and
            // advance to SHOWING_PLAYER.
            mPlayerBehaviourHandle.GetBehaviour()->RewindControllerToStart();   // controller += 0.0

            lrCamera.mState_uFlags |= 0x40;
            lrCamera.mState_uFlags |= 0x10000000;

            mfTimeInState  = 0.0f;   // +0x214 = 0
            muCurrentRival = 0;      // +0x218 = 0
            meState        = E_STATE_SHOWING_PLAYER;   // +0x220 = 3
            break;
        }

        case E_STATE_SHOWING_PLAYER:
        {
            // Drive the camera from the player "show" produced camera. When the show time has
            // elapsed, set up the rival movie + interpolate to it (MOVING_TO_RIVAL); otherwise
            // keep the camera behaviour-driven.
            lrCamera = mPlayerBehaviourHandle.GetProducedCamera();

            if (mfTimeInState < mfTimeToSpendLookingAtPlayer)
            {
                lrCamera.mState_uFlags |= 0x1000000;
            }
            else
            {
                SetupRivalMovie(lrSharedInfo, muCurrentRival);

                // Allocate the interpolator and seed it from the InterpolatorParameters block,
                // then blend FROM the rival "show" produced camera TO the player "show" produced
                // camera over the per-move time. (X360 builds the two CameraReferences from the
                // rival + player handles and latches Setup over them via sub_8224EE58; expressed
                // here through the BehaviourInterpolate named-setup API.)
                SetupInterpolator(lrSharedInfo,
                    maRivalBehaviourHandle[muRivalMovieOffset - 1].GetProducedCamera(),
                    mPlayerBehaviourHandle.GetProducedCamera());

                mfTimeInState = 0.0f;   // +0x214 = 0
                meState       = E_STATE_MOVING_TO_RIVAL;   // +0x220 = 5
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_SHOWING_RIVAL:
        {
            // Drive the camera from the interpolator while it runs, else from the current rival's
            // produced camera. Once the show time has elapsed and the interpolator is done, either
            // start the next rival (re-setup + interpolate) or, after the last rival, interpolate
            // back to the player (MOVING_TO_PLAYER).
            const bool lbInterpolatorActive = mInterpolator.IsAllocated();

            if (lbInterpolatorActive)
            {
                lrCamera = mInterpolator.GetProducedCamera();
                mfTimeInState = 0.0f;   // +0x214 = 0
                meState = E_STATE_MOVING_TO_RIVAL;   // re-enter the move (X360 *v3 = 5)
            }
            else
            {
                lrCamera = maRivalBehaviourHandle[muRivalMovieOffset - 1].GetProducedCamera();
            }

            if (lbInterpolatorActive || mfTimeInState < mfTimeToSpendLookingAtRival)
            {
                lrCamera.mState_uFlags |= 0x10000000;
                break;
            }

            const u32 luNextRival = muCurrentRival + 1;
            if (luNextRival >= lrGameState.muNumberOfCarsInIntro)
            {
                // Last rival: interpolate back to the player "show".
                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mPlayerBehaviourHandle, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);

                const Attrib::Gen::shotgroup& lrOnlineRaceStartShotGroup =
                    lrSharedInfo.mpDirectorResourceManager->GetOnlineRaceStartShots();
                const void* lpShotData =
                    const_cast<Attrib::Gen::shotgroup&>(lrOnlineRaceStartShotGroup)
                        .GetShotListData(/*liShotIndex*/ 1);
                if (!lpShotData)
                    lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

                Camera::BehaviourIceAnim* lpBehaviour = mPlayerBehaviourHandle.GetBehaviour();
                lpBehaviour->SetParameters(
                    static_cast<Camera::BehaviourIceAnim::ShotReference*>(const_cast<void*>(lpShotData)));
                lpBehaviour->SetPrimaryVehicleRefToPlayer();     // +0xDF0
                lpBehaviour->SetSecondaryVehicleRefToPlayer();   // +0xE00
                mPlayerBehaviourHandle.GetBehaviour()->SetForceMotionBlurEverything(true); // +0xE2B
                mPlayerBehaviourHandle.GetBehaviour()->ClearBaseFirstFrameGate();          // +0x28 = 0

                // Blend from the player "show" back over the last rival "show".
                SetupInterpolator(lrSharedInfo,
                    mPlayerBehaviourHandle.GetProducedCamera(),
                    maRivalBehaviourHandle[muRivalMovieOffset - 1].GetProducedCamera());

                // Release the rival behaviour (X360 sub_8222DFD8 on the rival handle).
                maRivalBehaviourHandle[muRivalMovieOffset - 1].Release();   // X360 sub_8222DFD8
                meState = E_STATE_MOVING_TO_PLAYER;   // +0x220 = 6
            }
            else
            {
                // Next rival: re-setup + interpolate from the player "show" to it.
                mfTimeInState  = 0.0f;
                muCurrentRival = luNextRival;
                SetupRivalMovie(lrSharedInfo, muCurrentRival);

                // Blend from the newly-set-up rival "show" over the previous one.
                SetupInterpolator(lrSharedInfo,
                    maRivalBehaviourHandle[muRivalMovieOffset - 1].GetProducedCamera(),
                    maRivalBehaviourHandle[muRivalMovieOffset - 2].GetProducedCamera());

                // Release the previous rival behaviour (X360 sub_8222DFD8).
                maRivalBehaviourHandle[muRivalMovieOffset - 2].Release();   // X360 sub_8222DFD8
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_MOVING_TO_RIVAL:
        {
            // Drive the camera from the interpolator. When the interpolator finishes, latch the
            // rival being shown + its show time on the shared info, release the interpolator and
            // advance to SHOWING_RIVAL.
            lrCamera = mInterpolator.GetProducedCamera();

            if (mInterpolator.GetBehaviour()->HasFinished())
            {
                // Report the rival now being shown to the director output interface (X360 writes
                // {requested 1, rival index, show time} into mpOutputInterface @+0x00/+0x04/+0x08
                // == ArbStateSharedInfo +0x10). FLAG: DirectorOutputInterface is forward-declared
                // only in this cone; the three fields are written by attested offset (opaque
                // external by documented offset, the accepted sibling pattern) -- replace with the
                // named accessors when the DirectorOutputInterface TU lands.
                s32* lpRivalReportSlot = reinterpret_cast<s32*>(lrSharedInfo.mpOutputInterface);
                lpRivalReportSlot[0] = 1;
                lpRivalReportSlot[1] = static_cast<s32>(muCurrentRival);
                *reinterpret_cast<f32*>(&lpRivalReportSlot[2]) = mfTimeToSpendLookingAtRival;

                mInterpolator.Release();   // X360 sub_8222DCA8 release

                lrCamera.mState_uFlags |= 0x2000000;
                mfTimeInState = 0.0f;   // +0x214 = 0
                meState       = E_STATE_SHOWING_RIVAL;   // +0x220 = 4
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_MOVING_TO_PLAYER:
        {
            // Drive the camera from the interpolator. When it finishes, advance to
            // SHOWING_PLAYER_AGAIN.
            lrCamera = mInterpolator.GetProducedCamera();

            if (mInterpolator.GetBehaviour()->HasFinished())
            {
                meState = E_STATE_SHOWING_PLAYER_AGAIN;   // +0x220 = 7
                lrCamera.mState_uFlags |= 0x4000000;
                mfTimeInState = 0.0f;   // +0x214 = 0
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_SHOWING_PLAYER_AGAIN:
        {
            // Drive the camera from the player "show" produced camera. Once the event reaches the
            // countdown, allocate + configure the start-lights behaviour and advance to
            // SHOWING_LIGHTS.
            lrCamera = mPlayerBehaviourHandle.GetProducedCamera();

            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_COUNTDOWN)
            {
                const Attrib::Gen::shotgroup& lrOnlineRaceStartShotGroup =
                    lrSharedInfo.mpDirectorResourceManager->GetOnlineRaceStartShots();

                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mLightsBehaviourHandle, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);
                mLightsBehaviourHandle.GetBehaviour()->SetForceMotionBlurEverything(true);   // +0xE2B = 1

                const void* lpShotData =
                    const_cast<Attrib::Gen::shotgroup&>(lrOnlineRaceStartShotGroup)
                        .GetShotListData(/*liShotIndex*/ 0);
                if (!lpShotData)
                    lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

                mLightsBehaviourHandle.GetBehaviour()->SetParameters(
                    static_cast<Camera::BehaviourIceAnim::ShotReference*>(const_cast<void*>(lpShotData)));

                meState = E_STATE_SHOWING_LIGHTS;   // +0x220 = 9
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_WAITING_FOR_COUNTDOWN:
        {
            // No cars to show: drive the camera from the selected gameplay camera. Once the event
            // reaches the countdown, allocate + configure the start-lights behaviour and advance
            // to SHOWING_LIGHTS.
            lrCamera = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();

            if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_COUNTDOWN)
            {
                const Attrib::Gen::shotgroup& lrOnlineRaceStartShotGroup =
                    lrSharedInfo.mpDirectorResourceManager->GetOnlineRaceStartShots();

                lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                    mLightsBehaviourHandle, this, KPC_NEW_BEHAVIOUR_OWNER, KI_NEW_BEHAVIOUR_REFLIMIT);
                mLightsBehaviourHandle.GetBehaviour()->SetForceMotionBlurEverything(true);   // +0xE2B = 1

                const void* lpShotData =
                    const_cast<Attrib::Gen::shotgroup&>(lrOnlineRaceStartShotGroup)
                        .GetShotListData(/*liShotIndex*/ 0);
                if (!lpShotData)
                    lpShotData = Attrib::DefaultDataArea(KU_SHOT_DEFAULT_DATA_AREA_SIZE);

                mLightsBehaviourHandle.GetBehaviour()->SetParameters(
                    static_cast<Camera::BehaviourIceAnim::ShotReference*>(const_cast<void*>(lpShotData)));

                meState = E_STATE_SHOWING_LIGHTS;   // +0x220 = 9
            }

            lrCamera.mState_uFlags |= 0x10000000;
            break;
        }

        case E_STATE_SHOWING_LIGHTS:
        {
            // Drive the camera from the start-lights produced camera. Once the event leaves the
            // countdown, hand control back to the roaming state.
            lrCamera = mLightsBehaviourHandle.GetProducedCamera();

            if (lrGameState.mEventState.GetCurrent() != GameState::E_EVENT_STATE_COUNTDOWN)
            {
                ArbUtils::ChangeToState<EState>(
                    this, lrSharedInfo, ArbitratorStateContainer::E_STATE_ROAMING,
                    meState, E_STATE_CHANGING_TO_ROAMING);
            }
            break;
        }

        case E_STATE_CHANGING_TO_ROAMING:
        {
            // Latch the selected gameplay camera, then hand control back to roaming.
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

        // ---- epilogue (runs for every non-INACTIVE state) ----------------------------------
        // While ACTIVE-or-later, force the primary gameplay behaviour to finish so the intro keeps
        // control, and end the intro back to roaming once the event leaves the intro/countdown.
        if (meState >= E_STATE_ACTIVE)
        {
            lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();
        }

        if (meState >= E_STATE_ACTIVE)
        {
            const GameState::EEventState leEventState = lrGameState.mEventState.GetCurrent();
            if (leEventState != GameState::E_EVENT_STATE_COUNTDOWN &&
                leEventState != GameState::E_EVENT_STATE_INTRO)
            {
                meState = E_STATE_CHANGING_TO_ROAMING;   // +0x220 = 10
            }
        }

        // The fade: while the game says the intro may use the result bars, keep "BlackFadeIn_
        // Quick" stopped; otherwise, unless the camera is already behaviour-driven, copy the
        // selected gameplay camera and re-arm the quick black fade-in.
        if (lrGameState.mbOnlineRaceIntroCanUseBars)
        {
            Camera::EnsureEffectIsStopped(lrCamera, *lrSharedInfo.mpEffectInterface,
                                          KPC_HOOK_BLACK_FADE_IN_QUICK);
        }
        else if ((lrCamera.mState_uFlags & 2) == 0)
        {
            lrCamera = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();
            Camera::RequestStartEffectHookReset(lrCamera, KPC_HOOK_BLACK_FADE_IN_QUICK,
                                                KF_FULL_SHOW_TIME);
        }

        mfTimeInState += lrSharedInfo.mfSimTimestep;   // +0x214 += mfSimTimestep
    }
}
