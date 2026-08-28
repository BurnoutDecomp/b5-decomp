#include "GameSource/Director/Arbitrator/States/BrnArbStateCrashing.h"

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer
#include "GameSource/Director/BrnDirectorResourceManager.h"                     // GetTakendown()
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                     // NewBehaviour<>
#include "GameSource/Director/Camera/BrnBehaviourParameterBank.h"               // NamedParameters
#include "GameSource/Director/Camera/BrnCameraValidityAccount.h"                // ValidityAccount::Print
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"                // SharedCameraContainer
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"          // BehaviourIceAnim
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h" // BehaviourSpirallingDeathcam
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"                  // Camera::VehicleInfo (CanRun)
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"            // GameState
#include "GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.h"   // DebugPrinter / DebugLog
#include "GameSource/Director/MomentController/BrnMoment.h"                     // Moment / MomentBystanderSeesAction
#include "GameSource/Director/MomentController/Moments/BrnMomentTumbling.h"     // MomentTumbling
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"                 // EnsureEffectIsPlaying / StopCurrentEffect
#include "GameSource/Director/Utils/BrnVehicleRef.h"                            // VehicleRef (the stack ref)
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT

// ============================================================================
// GameSource/Director/Arbitrator/States/BrnArbStateCrashing.cpp
//
// ⭐⭐⭐ BrnDirector::ArbStateCrashing -- THE CRASH CAMERA, reconstructed from
// BURNOUT_X360_ARTIST.XEX. Nine ledger functions, ~1130 lines of asm:
//   Construct                   @0x82259EA0    Prepare                     @0x822655E8
//   Update                      @0x8226BFB0    Release                     @0x82234F70
//   CanRun                      @0x821F6258    GetName                     @0x821F6248
//   SelectNormalCrashCamera     @0x82254FB0    ProcessPossibleStateChanges @0x8224F5B0
//   ApplySlomoAndShake          @0x8224F8D8
//
// ⚠️ ApplySlomoAndShake is NOT in `work show`'s function list for this TU: its DecFIGS
// primary_file is CgsArray.h (the usual inlined-header misattribution), so a TU-scoped dossier
// misses the one function that carries the feature. It is here because the ARTIST symbol says
// BrnDirector::ArbStateCrashing::ApplySlomoAndShake, not because the file attribution did.
//
// ⛔⛔ THIS FILE AND MainDirector::ProcessInputQueue's mbCrashActive LEG ARE ONE CHANGE.
// Until this class existed, E_STATE_CRASHING's container slot was an EMPTY placeholder whose
// inherited Update does nothing and never writes meState -- and nothing wrote
// GameState::mbCrashActive, so ArbStateRoaming could never take the crash edge and the shell
// was never exercised. Landing the writer alone would have handed the first crash of the
// session to that shell: no Update, no state change, no exit edge, a camera frozen for the rest
// of the run out of a green build.
//
// ----------------------------------------------------------------------------
// TRANSCRIPTION NOTES (the places Hex-Rays is wrong here, all read off the asm):
//
//  * ApplySlomoAndShake's THREE predicates. The pseudocode emits `v7 = v5; if (v7 || ...)`,
//    aliasing the FIRST block into the test. The asm computes three separate values --
//    r28 = block1 @0x8224F930, r29 = block2 @0x8224F96C, then r10 = block2 and r29 = block3
//    @0x8224F9A4/A8 -- and the slow-motion test uses the SECOND.
//  * Neither controller call's argument list survives Hex-Rays: the f32 timestep takes f1 and
//    leaves the r5 GPR slot unwritten, so the decompiler stops tracking and drops four of the
//    six pointer arguments at each site. Both lists are taken from the asm.
//  * Hex-Rays' `field_3A8 / 3A9 / 3AA / 390 / 398 / 38C` are mMomentSelector's OWN members
//    (+0x1E0 / +0x1E1 / +0x1E2 / +0x1C8 / +0x1D0 / +0x1C4), not state members.
//  * The console re-issues GetSelectedMoment() before every single read (each carrying its own
//    HasSelectedMoment tripwire). Hoisted to one local per group here: same reads, same order.
// ============================================================================

namespace BrnDirector
{
    namespace
    {
        // ---- .rdata constants, all X360-attested ---------------------------------------
        // kfMomentTime / kfFailsafeTime are the two file-scope constants the DWARF declares at
        // BrnArbStateCrashing.h:88/:89 (`extern const float32_t`). Both compares in
        // SelectNormalCrashCamera / Update load flt_82001D9C.
        const f32 KF_MOMENT_TIME   = 2.0f;    // flt_82001D9C  kfMomentTime
        const f32 KF_FAILSAFE_TIME = 2.0f;    // flt_82001D9C  kfFailsafeTime

        // How long the AFTERCRASH arm holds the gameplay camera before interpolating home.
        const f32 KF_AFTERCRASH_TIME = 0.5f;  // flt_82CDAD90 (Update case 3)

        // The bystander moment's perceived-distance squash once it has held the crash for
        // longer than kfMomentTime -- pull the framing in so a long crash stays readable.
        const f32 KF_BYSTANDER_PERCEIVED_DISTANCE_FACTOR = 0.5f;   // flt_82001DA0

        // The moment weightings Construct registers with. The three tumbling/bystander
        // candidates share one register (f31, loaded once from flt_82004014); the hard stop
        // gets 1.0 (flt_82001C98), i.e. it is the default crash camera and the other three are
        // long shots.
        const f32 KF_LONG_SHOT_WEIGHTING = 0.1f;   // flt_82004014
        const f32 KF_HARD_STOP_WEIGHTING = 1.0f;   // flt_82001C98

        // How fast a moment's recency score decays between selections (flt_8200AE70). The same
        // value ArbStateTakedown::Construct @0x8225A318 uses.
        const f32 KF_RECENCY_FACTOR = 0.995f;

        // Only ONE of this state's moments may be live at a time.
        const u32 KU_MAX_ACTIVE_MOMENTS = 1;

        // How long the crash must still have left before the director is allowed to cut to a
        // different camera (Update passes `mfCrashTimeRemaining <= 1.0f` as
        // lbTooLateToSwitchCameras).
        const f32 KF_NO_CUT_CRASH_TIME_REMAINING = 1.0f;   // flt_82001C98

        // The state must have been running for at least this many frames before Prepare will
        // accept "no valid moments" as merely not-ready-yet rather than a failure.
        const u32 KU_MIN_FRAMES_BEFORE_MOMENT_CHECK = 2;

        // The camera-state flag Update raises on its produced camera every frame it runs.
        // FLAG: the bit VALUE is asm (`ld / ori 0x400 / std` at camera +0x140, the tail of
        //   Update @0x8226C610); the ROLE name is inferred from the only state that raises it.
        //   It is NOT the crash bit ArbStateRoaming's own gate tests -- that one is 0x8000.
        const s32 KI_CAMERA_STATE_CRASH_CAMERA_ACTIVE = 0x00000400;

        // The camera-state "follow the car" bit, cleared on the wrecked exit
        // (`ld / and 0xFFFFFFFD / std`). Same bit ArbStateCarSelect names KI_CAMERA_STATE_FOLLOW.
        const s32 KI_CAMERA_STATE_FOLLOW = 0x00000002;

        // The screen-effect hooks this state requests.
        const char* const KPC_HOOK_CRASH             = "Crash";
        const char* const KPC_HOOK_WRECKED           = "Wrecked";
        const char* const KPC_HOOK_CAR_RESET         = "Car_Reset";
        const char* const KPC_HOOK_BLACK_FADE_IN     = "BlackFadeIn_Quick";
        const f32         KF_EFFECT_BLEND            = 1.0f;   // flt_82001C98

        // The debug-log / debug-print colours (the raw RGBA immediates the asm passes).
        const CgsDev::RGBA KU_LOG_COLOUR_MOMENT = static_cast<CgsDev::RGBA>(-16776961);  // 0xFF0000FF
        const CgsDev::RGBA KU_LOG_COLOUR_TEXT   = static_cast<CgsDev::RGBA>(-1);         // 0xFFFFFFFF
        const CgsDev::RGBA KU_PRINT_COLOUR_OK   = static_cast<CgsDev::RGBA>(-16711936);  // 0xFF00FF00

        // The source-file string every assert in this TU cites.
        const char* const KPC_SOURCE_FILE =
            "..\\..\\..\\GameSource\\Director/Arbitrator/States/BrnArbStateCrashing.cpp";

        // The take the taken-down camera plays is the FIRST shot of the resource manager's
        // "takendown" shot group.
        const u32 KU_TAKENDOWN_SHOT_INDEX = 0;
    }

    // ------------------------------------------------------------------------
    // Construct @0x82259EA0
    //
    // Build the crash camera: construct the embedded camera, clear the base flags and the state,
    // then seed the moment selector with the FOUR candidate crash moments and its tuning.
    //
    // ⭐ The four candidates are the whole crash-camera vocabulary, and every one of their
    // parameter ids is a "*_CRASH_ONLY" enumerator -- which is an independent confirmation that
    // this transcription reads the right two words out of each 16-byte stack record:
    //     TUMBLING            + TUMBLING_LEAD_CRASH_ONLY           weight 0.1, inhibitable
    //     TUMBLING            + TUMBLING_TRUCKING_SIDE_CRASH_ONLY  weight 0.1, inhibitable
    //     HARD_STOP           + HARD_STOP_DEFAULT                  weight 1.0, NOT inhibitable
    //     BYSTANDER_SEES_ACTION + BYSTANDER_FAR_CRASH_ONLY         weight 0.1, NOT inhibitable
    // The console builds each record on the stack and passes it in the r4:r5 GPR pair (the
    // field-wise AddMoment overload, inlined at every site).
    // ------------------------------------------------------------------------
    void ArbStateCrashing::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // stb 0, +0x170 / +0x171

        meState = E_STATE_INACTIVE;        // stw 0, +0x3C8

        // The console inlines MomentSelector::Construct() over the embedded selector at +0x1C8
        // (the three Array count words + the scalar block), exactly as ArbStateRoaming does.
        mMomentSelector.Construct();

        mMomentSelector.AddMoment(Moment::E_MOMENT_TUMBLING,
                                  MomentParameterBank::E_PARAM_TUMBLING_LEAD_CRASH_ONLY,
                                  KF_LONG_SHOT_WEIGHTING, /*mbCanBeInhibited*/ true);
        mMomentSelector.AddMoment(Moment::E_MOMENT_TUMBLING,
                                  MomentParameterBank::E_PARAM_TUMBLING_TRUCKING_SIDE_CRASH_ONLY,
                                  KF_LONG_SHOT_WEIGHTING, /*mbCanBeInhibited*/ true);
        mMomentSelector.AddMoment(Moment::E_MOMENT_HARD_STOP,
                                  MomentParameterBank::E_PARAM_HARD_STOP_DEFAULT,
                                  KF_HARD_STOP_WEIGHTING, /*mbCanBeInhibited*/ false);
        mMomentSelector.AddMoment(Moment::E_MOMENT_BYSTANDER_SEES_ACTION,
                                  MomentParameterBank::E_PARAM_BYSTANDER_FAR_CRASH_ONLY,
                                  KF_LONG_SHOT_WEIGHTING, /*mbCanBeInhibited*/ false);

        mMomentSelector.SetRecencyFactor(KF_RECENCY_FACTOR);

        // A second, redundant store of meSelectionMode between SetRecencyFactor and
        // SetMaxActiveMoments (`stw 0, 0x3A4(this)` == selector +0x1DC, no call) -- an inlined
        // SetSelectionMode right after Construct()'s own seed.
        mMomentSelector.SetSelectionMode(MomentSelector::E_MODE_LRU_BEST);

        mMomentSelector.SetMaxActiveMoments(KU_MAX_ACTIVE_MOMENTS);

        // Both camera handles start unallocated (+0x180 / +0x194 blocks zeroed).
        mDeathcam     = Camera::BehaviourHandle<Camera::BehaviourSpirallingDeathcam>();
        mTakenDownCam = Camera::BehaviourHandle<Camera::BehaviourIceAnim>();
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6248
    // ------------------------------------------------------------------------
    const char* ArbStateCrashing::GetName() const
    {
        return "ArbStateCrashing";
    }

    // ------------------------------------------------------------------------
    // CanRun @0x821F6258 -- four instructions, and the whole crash chain turns on them:
    //     lwz r11, 0x48(sharedInfo)   ; mpPlayerCar
    //     lbz r3,  0x44A(r11)         ; +0x44A == 1098 == mRaceCarState.mbCrashing
    //     blr
    // ⭐ That +0x44A is the SECOND independent pin on the byte MainDirector::ProcessInputQueue
    // publishes into GameState::mbCrashActive (BrnVehicleEvents.h:88 is the first, mapping
    // serialised @1098 back to physics +0x710). The two ends of the crash gate read the same
    // byte off the same published record.
    // ------------------------------------------------------------------------
    bool ArbStateCrashing::CanRun(ArbStateSharedInfo& lrSharedInfo) const
    {
        return lrSharedInfo.mpPlayerCar->mRaceCarState.mbCrashing;
    }

    // ------------------------------------------------------------------------
    // Prepare @0x822655E8 -- enter the crashing state.
    //
    // Re-entering while already ACTIVE (or already on the way out) is a no-op success. Otherwise
    // latch what kind of crash this is, allocate the deathcam if the player has been totalled,
    // and prepare the moment selector. Two things can hold the state "not prepared yet": a fresh
    // deathcam allocation (it needs a frame), and having no valid moment during the first two
    // frames.
    // ------------------------------------------------------------------------
    bool ArbStateCrashing::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        if (meState == E_STATE_ACTIVE || meState == E_STATE_CHANGING_TO_ROAMING)
        {
            return true;
        }

        bool lbPrepared = true;

        meState = E_STATE_PREPARING;              // stw 1, +0x3C8

        mbPlayerWasWreckedThisCrash = false;      // stb 0, +0x3BF

        // GameState +0x1C1 == mbRoadRageTotalled: the spiralling deathcam is the totalled-car
        // camera, so the flag that decides whether to use it is the totalled flag.
        mbShouldUseDeathcam = lrSharedInfo.mpGameState->mbRoadRageTotalled;   // stb +0x3BE

        // The tracker classified this crash's energy band last frame (tracker +0x298).
        meCrashType = lrSharedInfo.mpPlayerTracker->GetCrashType();           // stw +0x3AC

        if (!mDeathcam.IsAllocated() && mbShouldUseDeathcam)
        {
            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourSpirallingDeathcam>(
                mDeathcam, this, 0, 1);
            mDeathcam.GetBehaviour()->SetParameters(
                &lrSharedInfo.mpNamedParameters->GetSpirallingDeathcamParameters());

            // A behaviour allocated this frame is not ready this frame.
            lbPrepared = false;
        }

        // The moment selector must come up, AND -- for the first two frames only -- it must
        // actually have a valid moment to offer. After two frames the state proceeds regardless
        // and SelectNormalCrashCamera's failsafe arm takes over.
        if (!mMomentSelector.Prepare(*lrSharedInfo.mpMomentController,
                                     *lrSharedInfo.mpBehaviourManager) ||
            (mMomentSelector.GetFramesActive() < static_cast<s32>(KU_MIN_FRAMES_BEFORE_MOMENT_CHECK) &&
             mMomentSelector.SnoopNumValidMoments() == 0))
        {
            lbPrepared = false;
        }

        return lbPrepared;
    }

    // ------------------------------------------------------------------------
    // ApplySlomoAndShake @0x8224F8D8
    //
    // ⭐⭐ THE CRASH SLOW MOTION. Every frame the state is ACTIVE and has not just changed
    // state, this either drives the two impact controllers or resets them, per the selected
    // moment and the crash's energy band.
    //
    // ⛔ HEX-RAYS GARBLES THE THREE PREDICATES -- it emits `v7 = v5` and tests the FIRST. The
    // asm computes three separate values off one shared sub-expression and the slow-motion test
    // uses the SECOND. All three share:
    //     lbHardStop = mMomentSelector.HasSelectedMoment() &&
    //                  GetSelectedMoment()->GetType() == E_MOMENT_HARD_STOP   // moment +0x170
    // and then:
    //     block1 = lbHardStop && meCrashType == E_CRASH_NORMAL       // r28 -> lbDontSetRealTime
    //     block2 = lbHardStop && meCrashType != E_CRASH_NORMAL       // r29@0x8224F96C -> suppress slomo
    //     block3 = lbHardStop && meCrashType == E_CRASH_HIGH_ENERGY  // r29@0x8224F9A8 -> suppress shake
    // Read as a sentence: a hard-stop moment on an ORDINARY crash keeps the slow motion but
    // asks it not to restore real time itself (the state's own exit will); a hard stop on any
    // other energy band gets no slow motion at all; a hard stop on a HIGH-energy crash also
    // gets no shake.
    //
    // The two "reset" arms are the controllers' own Constructs -- the slow-motion arm's three
    // stores are exactly mfTimeSinceLastSlomo = FLT_MAX / mfTimeInSlomo = 0 /
    // mbFirstFrameOfSlomo = false, and the shake arm's five zeroed floats are exactly a
    // CameraImpactEffect (one impact factor + a four-float CameraShake).
    // ------------------------------------------------------------------------
    void ArbStateCrashing::ApplySlomoAndShake(ArbStateSharedInfo& lrSharedInfo)
    {
        const GameState& lrGameState = *lrSharedInfo.mpGameState;

        const bool lbHardStopMoment =
            mMomentSelector.HasSelectedMoment() &&
            mMomentSelector.GetSelectedMoment()->GetType() == Moment::E_MOMENT_HARD_STOP;

        const bool lbDontSetRealTime =
            lbHardStopMoment && meCrashType == VehicleTracker::E_CRASH_NORMAL;
        const bool lbSuppressSlomo =
            lbHardStopMoment && meCrashType != VehicleTracker::E_CRASH_NORMAL;
        const bool lbSuppressShake =
            lbHardStopMoment && meCrashType == VehicleTracker::E_CRASH_HIGH_ENERGY;

        // A low-energy crash never slows down; neither does one during an event mode that owns
        // its own timing (meEventType 0 or 3), nor while the impact-time effect already holds
        // the sim clock.
        if (lbSuppressSlomo ||
            meCrashType == VehicleTracker::E_CRASH_LOW_ENERGY ||
            lrGameState.meEventType == 3 ||
            lrGameState.meEventType == 0 ||
            lrGameState.mbImpactTimeActive)
        {
            mImpactSlomoController.Construct();
        }
        else
        {
            // ⚠️ ARGUMENT LIST FROM THE ASM (@0x8224F9F0..0x8224FA08), not the pseudocode:
            //   r3 = &mImpactSlomoController   r4 = &mCamera          f1 = sharedInfo +0x5C
            //   r6 = sharedInfo +0x38          r7 = sharedInfo +0x3C  r8 = sharedInfo +0x04
            //   r9 = block1
            mImpactSlomoController.Update(GetNonConstCamera(),
                                          lrSharedInfo.mfTimestep,
                                          *lrSharedInfo.mpAllVehicleData,
                                          *lrSharedInfo.mpPlayerTracker,
                                          *lrSharedInfo.mpDebugPrinter,
                                          lbDontSetRealTime);
        }

        if (lbSuppressShake)
        {
            mImpactShakeController.Construct();
        }
        else
        {
            // The 7th argument is a 16-byte VehicleRef the console builds on the stack at
            // 0x8224FA78..0x8224FA88: { E_PLAYER_CAR, -1, 0, mbSet = true } -- a default
            // player-car reference, so the shake is always measured against the player's car.
            VehicleRef lPlayerRef;
            lPlayerRef.meType         = VehicleRef::E_PLAYER_CAR;   // stw 0,  var_50
            lPlayerRef.miRaceCarIndex = -1;                         // stw -1, var_4C
            lPlayerRef.muRef          = 0;                          // stw 0,  var_48
            lPlayerRef.mbSet          = true;                       // stb 1,  var_44

            // ⚠️ Same argument-list caveat (@0x8224FA54..0x8224FA8C):
            //   r3 = &mImpactShakeController   r4 = &mCamera          f1 = sharedInfo +0x5C
            //   r6 = sharedInfo +0x38          r7 = sharedInfo +0x3C  r8 = sharedInfo +0x28
            //   r9 = sharedInfo +0x04          r10 = &<the stack ref>
            mImpactShakeController.Update(GetNonConstCamera(),
                                          lrSharedInfo.mfTimestep,
                                          *lrSharedInfo.mpAllVehicleData,
                                          *lrSharedInfo.mpPlayerTracker,
                                          *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom),
                                          *lrSharedInfo.mpDebugPrinter,
                                          lPlayerRef);
        }
    }

    // ------------------------------------------------------------------------
    // SelectNormalCrashCamera @0x82254FB0
    //
    // Produce this frame's camera. Two arms, and they are the crash camera's whole cutting
    // policy:
    //
    //   A MOMENT IS SELECTED -- take its camera. If the moment has gone invalid, re-pick first
    //   (and reset both timers). Then, once the moment has held for longer than kfMomentTime AND
    //   it says the director may cut away from it AND the crash is not nearly over, re-pick
    //   again if any other moment is valid ("Got bored"). Advance mfMomentTimer.
    //
    //   NO MOMENT IS SELECTED -- fall back to the shared gameplay camera, and once the failsafe
    //   timer passes kfFailsafeTime (again, only while the crash has time left and some moment
    //   is valid) try to select one. Advance mfFailsafeTimer.
    //
    // Either way, a live taken-down (ICE) camera overrides the result -- unless the impact-time
    // effect owns the frame.
    // ------------------------------------------------------------------------
    void ArbStateCrashing::SelectNormalCrashCamera(ArbStateSharedInfo& lrSharedInfo,
                                                   bool lbTooLateToSwitchCameras)
    {
        // Re-pick immediately if the selected moment has stopped being valid.
        if (mMomentSelector.HasSelectedMoment() && !mMomentSelector.GetSelectedMoment()->IsValid())
        {
            if (IsDebugDisplayActive())
            {
                const Moment* lpMoment = mMomentSelector.GetSelectedMoment();
                lrSharedInfo.mpDebugLog->AppendName(*lpMoment, KU_LOG_COLOUR_MOMENT);
                lpMoment->GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugLog);
            }

            mMomentSelector.SelectNewBestMoment(
                *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));

            mfMomentTimer   = 0.0f;   // +0x3B4
            mfFailsafeTimer = 0.0f;   // +0x3B8
        }

        if (mMomentSelector.HasSelectedMoment())
        {
            GetNonConstCamera() = mMomentSelector.GetSelectedMoment()->GetCamera();

            if (mfMomentTimer > KF_MOMENT_TIME &&
                mMomentSelector.GetSelectedMoment()->CanSwitchFromMeNow() &&   // moment +0x179
                !lbTooLateToSwitchCameras)
            {
                if (mMomentSelector.GetNumValidMoments() != 0)
                {
                    if (IsDebugDisplayActive())
                    {
                        const Moment* lpMoment = mMomentSelector.GetSelectedMoment();
                        lrSharedInfo.mpDebugLog->AppendName(*lpMoment, KU_LOG_COLOUR_MOMENT);
                        lpMoment->GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugLog);
                        lrSharedInfo.mpDebugLog->Append("Got bored", KU_LOG_COLOUR_TEXT);
                    }

                    mMomentSelector.SelectNewBestMoment(
                        *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));

                    mfMomentTimer = 0.0f;
                }
            }
            else if (IsDebugDisplayActive())
            {
                const Moment* lpMoment = mMomentSelector.GetSelectedMoment();
                lrSharedInfo.mpDebugPrinter->PrintName(*lpMoment, KU_PRINT_COLOUR_OK);
                lpMoment->GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugPrinter);
                if (lbTooLateToSwitchCameras)
                {
                    lrSharedInfo.mpDebugPrinter->Print("NoCutFrom: End of crash");
                }
            }

            mbAftertouchingSinceStart = false;              // stb 0, +0x3BC
            mfMomentTimer += lrSharedInfo.mfTimestep;       // +0x3B4
        }
        else
        {
            // No moment: the shared gameplay camera IS the crash camera this frame. The console
            // takes the EXTERNAL (chase) handle's produced camera directly here, not the
            // flag-selected one (`addi r3, container, 4` with no lookback test).
            GetNonConstCamera() =
                lrSharedInfo.mpSharedCameraContainer->mGameplayExternal.GetProducedCamera();

            if (mMomentSelector.GetNumValidMoments() != 0 &&
                mfFailsafeTimer > KF_FAILSAFE_TIME &&
                !lbTooLateToSwitchCameras)
            {
                mMomentSelector.SelectBestMoment(
                    *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));
                mfMomentTimer = 0.0f;
            }
            else if (IsDebugDisplayActive())
            {
                lrSharedInfo.mpDebugPrinter->Print("Failsafe", KU_PRINT_COLOUR_OK);
                GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugPrinter);
                if (mMomentSelector.GetNumValidMoments() == 0)
                {
                    lrSharedInfo.mpDebugPrinter->Print("No valid moments");
                }
                if (mfFailsafeTimer > KF_FAILSAFE_TIME)
                {
                    lrSharedInfo.mpDebugPrinter->Print("Failsafe timer elapsed");
                }
                if (lbTooLateToSwitchCameras)
                {
                    lrSharedInfo.mpDebugPrinter->Print("NoCutFrom: End of crash");
                }
            }

            mbAftertouchingSinceStart = false;              // stb 0, +0x3BC
            mfFailsafeTimer += lrSharedInfo.mfTimestep;     // +0x3B8
        }

        // A live taken-down camera wins over whatever the moment/failsafe arm just produced --
        // unless the impact-time effect is running (GameState +0x105), which owns the frame.
        if (!lrSharedInfo.mpGameState->mbImpactTimeActive &&
            mTakenDownCam.IsAllocated() &&
            !mTakenDownCam.IsWaitingToPrepare() &&
            (mTakenDownCam.GetProducedCamera().mState_uFlags & KI_CAMERA_STATE_FOLLOW) != 0)
        {
            GetNonConstCamera() = mTakenDownCam.GetProducedCamera();
        }
    }

    // ------------------------------------------------------------------------
    // ProcessPossibleStateChanges @0x8224F5B0
    //
    // ⭐ THE EXIT EDGE. Two mutually exclusive top-level arms:
    //
    //   1. The intro has asked to go to crash mode AND the event is in its third state: hand the
    //      container to ArbStateCrashMode (the debug line is literally "Switching to crash mode")
    //      and Release() this state.
    //   2. The crash is over (GameState::mbCrashActive has dropped): pick which way out --
    //        wrecked                             -> CHANGING_TO_ROAMING behind "BlackFadeIn_Quick"
    //        the primary gameplay camera is live -> AFTERCRASH_SLOW on a low-energy hard stop,
    //                                               else AFTERCRASH
    //        the primary gameplay camera is NOT  -> CHANGING_TO_ROAMING behind "Car_Reset"
    //
    // While the crash IS active neither arm fires and the state just keeps running -- which is
    // exactly why mbCrashActive had to become real before this class could do anything.
    // ------------------------------------------------------------------------
    void ArbStateCrashing::ProcessPossibleStateChanges(ArbStateSharedInfo& lrSharedInfo,
                                                       bool* lpbHasChangedState)
    {
        if (lpbHasChangedState == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpbHasChangedState != NULL", KPC_SOURCE_FILE, 569);
            CgsDev::Assert::EndAssert();
        }

        *lpbHasChangedState = false;

        GameState&                lrGameState = *lrSharedInfo.mpGameState;
        ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;

        if (lrGameState.mbGoToCrashModeAfterIntro &&                                // +0x11C
            lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_ACTIVE) // +0x124 == 3
        {
            GetNonConstCamera().ClearCrashNavEffectGate();     // stb 0, camera +0x11F
            Camera::StopCurrentEffect(GetNonConstCamera(), *lrSharedInfo.mpEffectInterface);

            if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_CRASH_MODE)
                    ->Prepare(lrSharedInfo))
            {
                if (mMomentSelector.HasSelectedMoment())
                {
                    if (IsDebugDisplayActive())
                    {
                        const Moment* lpMoment = mMomentSelector.GetSelectedMoment();
                        lrSharedInfo.mpDebugLog->AppendName(*lpMoment, KU_LOG_COLOUR_MOMENT);
                        lpMoment->GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugLog);
                        lrSharedInfo.mpDebugLog->Append("Switching to crash mode", KU_LOG_COLOUR_TEXT);
                    }

                    mMomentSelector.CancelSelection();
                }

                // container +13772 (mpCurrentState) = container +13744 (state table[4]).
                lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_CRASH_MODE);
                Release(lrSharedInfo);          // the console dispatches vtable slot 3
                *lpbHasChangedState = true;
            }
        }
        else if (!lrGameState.mbCrashActive)     // +0xF9 -- the crash has ended
        {
            if (mbPlayerWasWreckedThisCrash)
            {
                if (mMomentSelector.HasSelectedMoment())
                {
                    mMomentSelector.CancelSelection();
                }

                // Re-arm the chase camera behind the fade so it snaps back to the car instead of
                // easing in from wherever the crash left it (the same three stores
                // SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish makes).
                lrSharedInfo.mpSharedCameraContainer->ForcePrimaryGameplayBehaviourToFinish();

                Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                              *lrSharedInfo.mpEffectInterface,
                                              KPC_HOOK_BLACK_FADE_IN, KF_EFFECT_BLEND);

                GetNonConstCamera().mState_uFlags &= ~KI_CAMERA_STATE_FOLLOW;

                meState = E_STATE_CHANGING_TO_ROAMING;
                *lpbHasChangedState = true;
            }
            else
            {
                // Is the shared gameplay camera's PRIMARY (external/chase) behaviour the live
                // one this frame? Same predicate SharedCameraContainer::GetSelectedGameplayCamera
                // uses: `mbUseGameplayExternal && !mbLookbackOverride`.
                const SharedCameraContainer& lrCameras = *lrSharedInfo.mpSharedCameraContainer;
                const bool lbPrimaryGameplayCameraLive =
                    lrCameras.mbUseGameplayExternal && !lrCameras.mbLookbackOverride;

                if (lbPrimaryGameplayCameraLive)
                {
                    if (meCrashType == VehicleTracker::E_CRASH_LOW_ENERGY &&
                        mMomentSelector.HasSelectedMoment() &&
                        mMomentSelector.GetSelectedMoment()->GetType() == Moment::E_MOMENT_HARD_STOP)
                    {
                        // A gentle hard stop: hold the moment's camera a moment longer.
                        GetNonConstCamera().ClearCrashNavEffectGate();
                        Camera::StopCurrentEffect(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface);
                        mfTimeActive = 0.0f;
                        meState      = E_STATE_AFTERCRASH_SLOW;
                    }
                    else
                    {
                        if (IsDebugDisplayActive() && mMomentSelector.HasSelectedMoment())
                        {
                            const Moment* lpMoment = mMomentSelector.GetSelectedMoment();
                            lrSharedInfo.mpDebugLog->AppendName(*lpMoment, KU_LOG_COLOUR_MOMENT);
                            lpMoment->GetCamera().GetValidityAccount().Print(*lrSharedInfo.mpDebugLog);
                            lrSharedInfo.mpDebugLog->Append("End of crash", KU_LOG_COLOUR_TEXT);
                        }

                        if (mMomentSelector.HasSelectedMoment())
                        {
                            mMomentSelector.CancelSelection();
                        }

                        mfTimeActive = 0.0f;
                        meState      = E_STATE_AFTERCRASH;
                    }
                }
                else
                {
                    // The chase camera is not the live one (lookback, or the bumper cam): there
                    // is nothing to hold, so reset straight out.
                    Camera::EnsureEffectIsPlaying(GetNonConstCamera(),
                                                  *lrSharedInfo.mpEffectInterface,
                                                  KPC_HOOK_CAR_RESET, KF_EFFECT_BLEND);
                    meState = E_STATE_CHANGING_TO_ROAMING;
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // Update @0x8226BFB0 -- the per-frame state machine.
    //
    // The moment selector is ticked first (only once prepared), then one arm per state. Every
    // arm falls through to the same tail: raise the crash-camera bit on the produced camera.
    // ------------------------------------------------------------------------
    void ArbStateCrashing::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState&                lrGameState = *lrSharedInfo.mpGameState;
        ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;

        // selector +0x1E1 == mbPrepared.
        if (mMomentSelector.IsPrepared())
        {
            mMomentSelector.Update(lrSharedInfo.mfTimestep);
        }

        switch (meState)
        {
        case E_STATE_INACTIVE:
            // Nothing has entered this state yet: reset the selector's activity clock and get it
            // ready. (The two stores the console inlines here ARE MomentSelector::ResetTimeActive.)
            mMomentSelector.ResetTimeActive();
            mMomentSelector.Prepare(*lrSharedInfo.mpMomentController,
                                    *lrSharedInfo.mpBehaviourManager);
            break;

        case E_STATE_PREPARING:
            // The console dispatches Prepare through the vtable (`(*(*this + 4))(this, info)`).
            if (!Prepare(lrSharedInfo))
            {
                break;                 // still not ready -- nothing else runs this frame
            }

            if (!mMomentSelector.HasSelectedMoment())
            {
                mMomentSelector.SelectBestMoment(
                    *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));
            }

            mbAftertouchingSinceStart = false;   // +0x3BC
            mbAftertouchActivated     = false;   // +0x3BD
            mbPlayerWasTakenDown      = false;   // +0x3C0
            mfMomentTimer             = 0.0f;    // +0x3B4
            meState                   = E_STATE_ACTIVE;
            mfFailsafeTimer           = 0.0f;    // +0x3B8
            mfTimeActive              = 0.0f;    // +0x3B0

            // Both controllers start clean for this crash (the console inlines their
            // Constructs -- three stores then five).
            mImpactSlomoController.Construct();
            mImpactShakeController.Construct();

            // ⭐ AND FALL THROUGH into the ACTIVE arm on the SAME frame. This is the console's
            // own `goto LABEL_9` out of case 1 into case 2 (@0x8226C0B8 -> loc_8226C0D8), not a
            // convenience: it is why the FIRST frame of a crash already produces a camera
            // instead of showing one stale frame of whatever was on screen.
            // fall through

        case E_STATE_ACTIVE:
            UpdateActive(lrSharedInfo, lrGameState);
            break;

        case E_STATE_AFTERCRASH:
            // Hold the chase camera for kfAfterCrashTime, then start interpolating home.
            GetNonConstCamera() =
                lrSharedInfo.mpSharedCameraContainer->mGameplayExternal.GetProducedCamera();

            if (mfTimeActive > KF_AFTERCRASH_TIME)
            {
                GetNonConstCamera().ClearCrashNavEffectGate();   // stb 0, camera +0x11F
                Camera::StopCurrentEffect(GetNonConstCamera(), *lrSharedInfo.mpEffectInterface);
                meState = E_STATE_INTERPOLATING_TO_ROAMING;
            }

            mfTimeActive += lrSharedInfo.mfTimestep;
            break;

        case E_STATE_AFTERCRASH_SLOW:
            // Stay on the selected moment's camera while it is still valid; the frame it goes
            // invalid, drop it and try to hand the container back to roaming.
            if (mMomentSelector.GetSelectedMoment()->IsValid())
            {
                GetNonConstCamera() = mMomentSelector.GetSelectedMoment()->GetCamera();
            }
            else
            {
                mMomentSelector.CancelSelection();
                GetNonConstCamera() =
                    lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();

                if (!ChangeToRoaming(lrSharedInfo, lrContainer))
                {
                    meState = E_STATE_CHANGING_TO_ROAMING;
                }
            }
            break;

        case E_STATE_INTERPOLATING_TO_ROAMING:
            GetNonConstCamera() =
                lrSharedInfo.mpSharedCameraContainer->mGameplayExternal.GetProducedCamera();

            if (!ChangeToRoaming(lrSharedInfo, lrContainer))
            {
                meState = E_STATE_CHANGING_TO_ROAMING;
            }
            break;

        case E_STATE_CHANGING_TO_ROAMING:
            GetNonConstCamera() =
                lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();

            if (lrContainer.GetState(ArbitratorStateContainer::E_STATE_ROAMING)
                    ->Prepare(lrSharedInfo))
            {
                // Only this arm drops a lingering selection before handing over.
                if (mMomentSelector.HasSelectedMoment())
                {
                    mMomentSelector.CancelSelection();
                }

                lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
                Release(lrSharedInfo);
            }
            break;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("unhandled state", KPC_SOURCE_FILE, 441);
            CgsDev::Assert::EndAssert();
            break;
        }

        // Every arm ends here: mark the produced camera as the crash camera's
        // (`ld / ori 0x400 / std` at camera +0x140).
        GetNonConstCamera().mState_uFlags |= KI_CAMERA_STATE_CRASH_CAMERA_ACTIVE;
    }

    // ------------------------------------------------------------------------
    // The ACTIVE arm of Update, de-inlined. The console reaches it twice -- once by falling out
    // of the PREPARING arm on the frame Prepare succeeds (`goto LABEL_9`), once as case 2 -- so
    // it is one block of source reached two ways, not two copies.
    // ------------------------------------------------------------------------
    void ArbStateCrashing::UpdateActive(ArbStateSharedInfo& lrSharedInfo, GameState& lrGameState)
    {
        if (IsDebugDisplayActive())
        {
            if (lrSharedInfo.mpDebugPrinter == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lSharedInfo.mpDebugPrinter != NULL", KPC_SOURCE_FILE, 273);
                CgsDev::Assert::EndAssert();
            }
            lrSharedInfo.mpDebugPrinter->Print("");
            mMomentSelector.DebugRender(*lrSharedInfo.mpDebugPrinter);
            lrSharedInfo.mpDebugPrinter->Print("");
        }

        // The player has just been taken down: bring up the authored takedown ICE camera,
        // anchored on whoever did it.
        if (lrGameState.mbPlayerWasTakenDown && !mTakenDownCam.IsAllocated())   // GameState +0x1C3
        {
            mbPlayerWasTakenDown = true;                            // +0x3C0
            mePlayerKillerIndex  = lrGameState.mePlayerKillerIndex; // +0x3C4 <- GameState +0x1C4

            lrSharedInfo.mpBehaviourManager->NewBehaviour<Camera::BehaviourIceAnim>(
                mTakenDownCam, this, 0, 1);

            const Attrib::Gen::shotgroup& lrTakendown =
                lrSharedInfo.mpDirectorResourceManager->GetTakendown();   // resources +0x618

            if (lrTakendown.Num_ShotList() == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "lSharedInfo.mpDirectorResourceManager->GetTakendown().Num_ShotList()>0",
                    KPC_SOURCE_FILE, 289);
                CgsDev::Assert::EndAssert();
            }

            // The indexed ShotList element (attribute key 0x15246B49, index 0), with the
            // generated accessor's own 24-byte DefaultDataArea fallback when it is absent.
            Camera::BehaviourIceAnim::ShotReference* lpShot =
                static_cast<Camera::BehaviourIceAnim::ShotReference*>(
                    lrTakendown.GetShotListElement(KU_TAKENDOWN_SHOT_INDEX));

            Camera::BehaviourIceAnim* lpTakenDownCam = mTakenDownCam.GetBehaviour();
            lpTakenDownCam->SetParameters(lpShot);
            lpTakenDownCam->SetSecondaryVehicleRefToRaceCar(mePlayerKillerIndex); // behaviour +0xE00
            lpTakenDownCam->SetUseCollisionPolicy(true);                          // behaviour +0xE28
            lpTakenDownCam->SetCollisionPolicyCanFail(false);                     // behaviour +0x28
        }

        // GameState +0xFC: how much crash is left. Under a second and the director stops cutting.
        SelectNormalCrashCamera(lrSharedInfo,
                                lrGameState.mfCrashTimeRemaining <= KF_NO_CUT_CRASH_TIME_REMAINING);

        // PlayerCrashInfo +0x26 -- "the player was WRECKED in this crash", which upgrades the
        // screen effect from "Crash" to "Wrecked" and latches the wrecked exit in
        // ProcessPossibleStateChanges.
        const char* lpcEffectHook = KPC_HOOK_CRASH;
        if (lrSharedInfo.mpPlayerCrashInfo->mbWrecked)
        {
            mbPlayerWasWreckedThisCrash = true;     // +0x3BF
            lpcEffectHook               = KPC_HOOK_WRECKED;
        }

        Camera::EnsureEffectIsPlaying(GetNonConstCamera(), *lrSharedInfo.mpEffectInterface,
                                      lpcEffectHook, KF_EFFECT_BLEND);

        // A totalled player gets the spiralling deathcam over everything else.
        if (mbShouldUseDeathcam)     // +0x3BE
        {
            GetNonConstCamera() = mDeathcam.GetProducedCamera();

            if (!mDeathcam.GetBehaviour()->IsStarted())     // behaviour +0x2F4
            {
                mDeathcam.GetBehaviour()->Start();
            }
        }

        bool lbHasChangedState = false;
        ProcessPossibleStateChanges(lrSharedInfo, &lbHasChangedState);

        if (!lbHasChangedState)
        {
            ApplySlomoAndShake(lrSharedInfo);

            // The frame a slow-motion burst starts is a good time for a tumbling moment to
            // plant its camera (it knows the car is about to be readable).
            if (mImpactSlomoController.IsFirstFrameOfSlomo() &&
                mMomentSelector.HasSelectedMoment() &&
                mMomentSelector.GetSelectedMoment()->GetType() == Moment::E_MOMENT_TUMBLING &&
                mMomentSelector.GetSelectedMoment()->IsValid())
            {
                // The console calls the derived method on the base pointer after exactly this
                // type test; the downcast is the faithful de-inlining of that.
                static_cast<MomentTumbling*>(mMomentSelector.GetSelectedMoment())
                    ->SignalIsGoodTimeToPlant();
            }

            // A bystander shot that has held the crash for longer than kfMomentTime pulls its
            // framing in so the car stays readable.
            if (mMomentSelector.HasSelectedMoment() &&
                mMomentSelector.GetSelectedMoment()->GetType() == Moment::E_MOMENT_BYSTANDER_SEES_ACTION &&
                mMomentSelector.GetSelectedMoment()->IsValid() &&
                mfMomentTimer > KF_MOMENT_TIME)
            {
                static_cast<MomentBystanderSeesAction*>(mMomentSelector.GetSelectedMoment())
                    ->SetPerceivedDistanceModificationFactor(KF_BYSTANDER_PERCEIVED_DISTANCE_FACTOR);
            }

            mfTimeActive += lrSharedInfo.mfTimestep;   // +0x3B0
        }
    }

    // ------------------------------------------------------------------------
    // The shared "try to hand the container back to ArbStateRoaming" edge, de-inlined from the
    // three arms that emit it (Update cases 4 / 5 / 6). Returns whether the hand-over happened.
    // The console's shape at each site is
    //     if (roaming->Prepare(info)) { container.mpCurrentState = table[E_STATE_ROAMING];
    //                                   this->Release(info); }
    // -- i.e. ArbUtils::ChangeToState's body without the CanRun tripwire, which is why this is
    // written out rather than routed through that template.
    // ------------------------------------------------------------------------
    bool ArbStateCrashing::ChangeToRoaming(ArbStateSharedInfo& lrSharedInfo,
                                           ArbitratorStateContainer& lrContainer)
    {
        if (!lrContainer.GetState(ArbitratorStateContainer::E_STATE_ROAMING)->Prepare(lrSharedInfo))
        {
            return false;
        }

        lrContainer.SetCurrentState(ArbitratorStateContainer::E_STATE_ROAMING);
        Release(lrSharedInfo);
        return true;
    }

    // ------------------------------------------------------------------------
    // Release @0x82234F70 -- leave the state: drop back to INACTIVE, hand both camera
    // behaviours back to the manager, and assert nothing is still allocated to us.
    // ------------------------------------------------------------------------
    bool ArbStateCrashing::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        meState = E_STATE_INACTIVE;   // stw 0, +0x3C8

        mDeathcam.Release();          // +0x180 block
        mTakenDownCam.Release();      // +0x194 block

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;                  // li r3, 1
    }
}
