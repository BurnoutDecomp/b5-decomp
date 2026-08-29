#include "GameSource/Director/Arbitrator/States/BrnArbStateRoaming.h"

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h"     // ArbUtils::ChangeToStateWithoutRelease
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h" // ArbitratorStateContainer::GetState
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"       // BrnDirector::GameState
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"           // AllVehicleData distance queries
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"           // VehicleTracker::GetRaceEndEffectAmount
#include "GameSource/Director/Utils/BrnDirectorEffectTrigger.h"            // Camera effect-hook free functions
#include "GameSource/Director/Camera/BrnSharedCameraContainer.h"           // SharedCameraContainer (Prepare camera select)
#include "GameSource/Director/MomentController/BrnMoment.h"                // Moment::EState / EType
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                // BrnNetwork::EPaybackType
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT (Update's default arm)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // [DIAG] gpDebugPrint (bring-up only)
#include <cstring>                                                         // strcmp (payback name compares)
#include <cstdlib>                                                         // [DIAG] getenv (bring-up only)

// ============================================================================
// BrnDirector::ArbStateRoaming -- reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity)
//   Construct                    @0x82259C00
//   GetName                      @0x821F6238
//   Prepare                      @0x82259D58
//   ProcessPossibleFX            @0x82234A00
//   ProcessPossibleStateChanges  @0x82219C58
//   Release                      @0x82234930
//
// The "free roam / driving" director state: it owns the establishing-shot MomentSelector, the
// per-event camera-PFX requests, and the transitions out to the other arbitrator states. All
// member access is BY NAME. The GameState snapshot it reacts to (lrSharedInfo.mpGameState) is
// reached through the named BrnDirector::GameState members; several of its console offsets map
// to a member only by inference (the X360 4-byte-pointer layout vs. the host model) -- those
// are FLAGged inline for the reviewer.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace
    {
        // Tuning literals the X360 loads from the .rdata float pool (the IEEE-754 patterns the
        // asm references). KF_UNIT is the 1.0f the bulk of the effect requests blend at.
        const f32 KF_UNIT                    = 1.0f;        // flt_82001C98
        const f32 KF_DAMAGE_CRIT_BASE_BLEND  = 0.75f;       // flt_82004018
        const f32 KF_STUNT_LANDED_FLASH      = 1.25f;       // flt_820092CC
        const f32 KF_NEAREST_CAR_FX_RANGE_SQ = 10000.0f;    // flt_82005D9C / flt_820092EC
        const f32 KF_NEAREST_CAR_FX_FALLOFF  = 3.3333334e-5f; // flt_82001770 group (1/30000)
        const f32 KF_FP_EPSILON              = 1.1920929e-7f;  // flt_82002514 (the fsel dead-zone)
        // ⭐ PINNED 2026-08-01 from the .rdata image. All four were carried as 1.0f -- a
        // "looks like the other blend amounts" default, not a read value -- and all four are
        // wrong. Same recalibrated .id1 reader as KF_IDLE_TIME_BEFORE_PICTURE_PARADISE below;
        // it independently reproduces five constants this subsystem had already derived by
        // other means (flt_82CDA4E4 == 3.0, flt_82CDADA4 == 10.0, flt_82CDA4EC == 0.1,
        // flt_82CDADA8 == 0.5, flt_82CDADA0 == 2.0 -- BrnArbStateCarSelect.cpp:94-104).
        const f32 KF_SHOWTIME_BORDERS_DUR    = 2.0f;        // flt_82CDA498 (borders duration)
        const f32 KF_SHOWTIME_BLUR_RAMP      = 0.8f;        // flt_82CDA4A0 (blur ramp-in speed)
        const f32 KF_SHOWTIME_BLUR_MAX       = 0.7f;        // flt_82CDA49C (blur maximum)
        const f32 KF_SHOWTIME_POSTFX_SCALE   = 0.15f;       // flt_82CDA5E0 (post-FX scale)

        // The X360 dirty-flag bits the FX path toggles in the camera state's flag word.
        const u32 KU_DIRTY_RACE_DAY_ACTIVE   = 0x80000;
        const u32 KU_DIRTY_CRASH_FLAG        = 0x8000;

        // Showtime (Picture Paradise) entry gate: the player-inactive-time threshold the X360
        // compares mfPlayerInactiveTime against (asm 0x82219F30 fcmpu vs flt_82CDA494). The DWARF
        // names this KF_IDLE_TIME_BEFORE_PICTURE_PARADISE (BrnArbStateRoaming.h:34).
        //
        // ⭐⭐ PINNED 2026-08-01 -- and the placeholder it replaces was ACTIVELY WRONG.
        // flt_82CDA494 == 0x43340000 == 180.0f, read out of the .rdata image (the unpacked
        // IDA .id1 flag array, recalibrated this wave -- its address mapping was off by 1594
        // bytes, which is why an earlier pass reported this word "not found"; the new
        // calibration is agreed by NINE independent function prologues and independently
        // reproduces flt_82001C98 == 1.0f and flt_82001CC0 == 0.0f).
        // ⚠️ WHY THE PLACEHOLDER MATTERED: at 0.0f the guard below reads
        // `mfPlayerInactiveTime > 0.0f`, i.e. it fires on the FIRST idle frame. The console
        // wants THREE MINUTES. With the placeholder in place, the moment this ladder started
        // running the arbitrator would have dived into Picture Paradise instead of handing off
        // to the junkyard / car-select state.
        const f32 KF_IDLE_TIME_BEFORE_PICTURE_PARADISE = 180.0f;  // flt_82CDA494

        // Construct's three establishing-shot candidate weights: ONE register (f31) loaded once
        // from flt_82001DA0 and reused for all three records. Read 2026-08-01.
        const f32 KF_MOMENT_WEIGHTING = 0.5f;               // flt_82001DA0

        // Update's prologue shake-fade divisors. Both are WRITABLE .data words at 0x82CDAD88 /
        // 0x82CDAD84 (not .rdata), both 1.0f in the shipped image, and the console really does
        // divide by them -- they are not a folded multiply. Faithful code keeps the division.
        const f32 KF_IMPACT_SHAKE_FADE_TIME = 1.0f;         // flt_82CDAD88
        const f32 KF_NORMAL_SHAKE_FADE_TIME = 1.0f;         // flt_82CDAD84
    }

    // ------------------------------------------------------------------------
    // Construct @0x82259C00 -- build the camera, zero the per-state flags/timers, and seed the
    // establishing-shot selector with the three candidate moments.
    // ------------------------------------------------------------------------
    void ArbStateRoaming::Construct()
    {
        GetNonConstCamera().Construct();   // X360 Camera::Construct(this+0x10)

        ResetBaseCameraFlags();            // stb 0, +0x170 / +0x171 @0x82259C5C..0x82259C60

        meState = E_STATE_PREPARING;       // +0x3BC = 1

        mbHasReversed            = false;  // +0x39C
        mbStartedJumpEffect      = false;  // +0x39D
        mbUsingInterpolator      = false;  // +0x39E
        mbDisablePictureParadise = false;  // +0x39F
        mbPlayRaceEndEffect      = false;  // +0x3A0

        // The two behaviour handles start unallocated (+0x180 / +0x194 blocks zeroed).
        mInterpolater  = BehaviourHandle<Camera::BehaviourInterpolate>();
        mRoadRunnerCam = BehaviourHandle<Camera::BehaviourRoadRunner>();

        // mInterpolateParams (+0x1A8): the X360 seeds the four selector words; the two
        // overlapping stores to the curve/method words settle on {8, 0, 0, 3}.
        mInterpolateParams.mauParams[0] = 8u;
        mInterpolateParams.mauParams[1] = 0u;
        mInterpolateParams.mauParams[2] = 0u;
        mInterpolateParams.mauParams[3] = 3u;

        // Scalar block zeros (+0x3A4..+0x3C8).
        mfTimeInShowtimeIntro = 0.0f;
        mfBordersTime         = 0.0f;
        mfImpactShakeAmount   = 0.0f;
        mfNormalShakeAmount   = 0.0f;
        mfTimeUntilNextStrobe = 0.0f;
        miStrobeCount         = 0;
        mfStateTimer          = 0.0f;
        mu8ImpactShakeType    = 0;
        mfWreckedEffectAmount = 0.0f;

        // ⭐ ADDED 2026-08-01: the console inlines MomentSelector::Construct() over the embedded
        // selector at +0x1B8 (@0x82259CD0..0x82259CF8 -- the three Array count words at +0x0A0 /
        // +0x194 / +0x1C0 and the scalar block). It was MISSING here, which meant the three
        // arrays were never Constructed: AddMoment's own "Array used before Construct/Clear was
        // called" tripwire would have fired on the first candidate, and muValidMoments (which
        // Update's DRIVING arm reads every frame) started as whatever the pool slot held.
        mMomentSelector.Construct();

        // Seed the establishing-shot candidates. The X360 builds a 16-byte MomentDescription on
        // the stack for each and passes it in the r4:r5 GPR pair (the field-wise AddMoment
        // overload, inlined). RE-READ 2026-08-01 off @0x82259C00 -- the three records are
        //   {type 7 PLAYER_JUMPING, param 0, weight flt_82001DA0, canBeInhibited false}
        //   {type 8 PLAYER_STUNT,   param 0, weight flt_82001DA0, canBeInhibited false}
        //   {type 10 NEW_CAR_JOINED,param 0, weight flt_82001DA0, canBeInhibited false}
        // ⚠️ The previous transcription ("types 8/8/10 with weights 0.5/0.5/0") was wrong on the
        // first TYPE and on two of the three WEIGHTS, and it called a two-argument AddMoment that
        // does not exist -- this TU did not compile. All three records share ONE weight register
        // (f31, loaded once at 0x82259C40 from flt_82001DA0 == 0.5f, read out of the .rdata image
        // with scratchpad\afw_id1b.py) and all three store 0 into the trailing bool.
        mMomentSelector.AddMoment(Moment::E_MOMENT_PLAYER_JUMPING,  MomentParameterBank::E_PARAM_NONE,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);
        mMomentSelector.AddMoment(Moment::E_MOMENT_PLAYER_STUNT,    MomentParameterBank::E_PARAM_NONE,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);
        mMomentSelector.AddMoment(Moment::E_MOMENT_NEW_CAR_JOINED,  MomentParameterBank::E_PARAM_NONE,
                                  KF_MOMENT_WEIGHTING, /*mbCanBeInhibited*/ false);
    }

    // ------------------------------------------------------------------------
    // GetName @0x821F6238
    // ------------------------------------------------------------------------
    const char* ArbStateRoaming::GetName() const
    {
        return "ArbStateRoaming";
    }

    // ------------------------------------------------------------------------
    // Prepare @0x82259D58 -- enter the roaming state from INACTIVE/PREPARING/RELEASING.
    // ------------------------------------------------------------------------
    bool ArbStateRoaming::Prepare(ArbStateSharedInfo& lrSharedInfo)
    {
        // Only (re)prepare from one of the entry states (X360: meState < 2 || meState == 16).
        if (meState != E_STATE_INACTIVE &&
            meState != E_STATE_PREPARING &&
            meState != E_STATE_RELEASING)
        {
            return true;
        }

        bool lbPrepared = true;
        mbStartedJumpEffect = false;       // +0x39D

        // Latch the currently-selected gameplay camera into mCamera (primary when active and
        // not suspended, otherwise the secondary).
        GetNonConstCamera() = lrSharedInfo.mpSharedCameraContainer->GetSelectedGameplayCamera();

        if (meState != E_STATE_PREPARING)  // X360 forces it back to PREPARING
        {
            meState = E_STATE_PREPARING;
        }

        if (mMomentSelector.HasSelectedMoment())   // drop any lingering selection
        {
            mMomentSelector.CancelSelection();
        }

        // @0x82259E40: r4 == sharedInfo+0x20 (mpMomentController), r5 == sharedInfo+0x18
        // (mpBehaviourManager). ⚠️ The previous transcription passed *mpGameState as the second
        // argument -- a type error that never compiled.
        if (!mMomentSelector.Prepare(*lrSharedInfo.mpMomentController,
                                     *lrSharedInfo.mpBehaviourManager))
        {
            lbPrepared = false;
        }

        // Reset the per-state timers / shake / strobe block (X360 stores 0.0f / 1.0f patterns).
        miStrobeCount         = 0;         // +0x3B8 cleared
        mfImpactShakeAmount   = 0.0f;      // +0x3AC
        mfTimeInShowtimeIntro = 0.0f;      // +0x3A4
        mfBordersTime         = 0.0f;      // +0x3A8
        mfTimeUntilNextStrobe = 0.0f;      // +0x3B4
        mfNormalShakeAmount   = KF_UNIT;   // +0x3B0 = 1.0
        mfWreckedEffectAmount = KF_UNIT;   // +0x3C8 = 1.0
        mu8ImpactShakeType    = 7;         // +0x3C4

        return lbPrepared;
    }

    // ========================================================================
    // Update @0x822643A0 -- the roaming state machine. ⭐ NEW 2026-08-01.
    //
    // SIGNATURE FROM ASM: `mr r27, r4` / `mr r31, r3` and nothing else is consumed on entry;
    // f1 is never read (the timestep comes from `lfs f13, 0x5C(r27)` == lrSharedInfo.mfTimestep);
    // every exit is a bare `b __restgprlr_21` with no r3 set-up. So:
    //     void ArbStateRoaming::Update(ArbStateSharedInfo&)
    // Hex-Rays' `_DWORD* __fastcall(_DWORD* result, float* a2)` is noise on both counts.
    //
    // SHAPE: a flat 16-entry jump table on meState.
    //     lwz r11, 0x3BC(r31)  ; meState
    //     cmplwi cr6, r11, 0xF ; bgt cr6, def_82264444
    //     jpt_82264444 @0x82264448, 16 entries (0x40 bytes)
    // ⚠️ E_STATE_RELEASING (16) is deliberately OUTSIDE the table -- it reaches the `default`
    // arm, which fires CGS_ASSERT(false, "unhandled state") (BrnArbStateRoaming.cpp:643 ==
    // 0x283) and falls into the shared epilogue. The EState enum is NOT wrong; the table is
    // just 16 wide. Do not add `case E_STATE_RELEASING`.
    //
    // ⚠️ ARMS 3/4/5/6 (the Picture-Paradise / idle group) ARE DEGRADED IN THIS BUILD -- see
    // the marked block below. Everything else is the console's own code.
    // ========================================================================
    void ArbStateRoaming::Update(ArbStateSharedInfo& lrSharedInfo)
    {
        SharedCameraContainer& lrContainer = *lrSharedInfo.mpSharedCameraContainer;

        // [diag] BRN_CRASHCAM_DIAG -- roaming's own meState edge. BRN_DIRECTOR_TRACE already
        // reports it, but flow_run.ps1 CLEARS that variable on every run, so a crash-camera
        // investigation cannot see the one state that decides whether the crash edge is even
        // reached. Edge-triggered; off unless the variable is set.
        if (getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siLastRoamState = -1;
            if (static_cast<s32>(meState) != siLastRoamState)
            {
                siLastRoamState = static_cast<s32>(meState);
                *CgsDev::Log::gpDebugPrint
                    << "[crashcam] roaming meState -> " << siLastRoamState << "\n";
            }
        }

        // ---- prologue @0x822643B8..0x8226441C: the impact/normal shake cross-fade. Runs for
        // EVERY state, before the switch. Both divisors are writable .data words (1.0f in the
        // shipped image) and the console really divides -- keep the division.
        mfImpactShakeAmount -= lrSharedInfo.mfTimestep / KF_IMPACT_SHAKE_FADE_TIME;   // +0x3AC
        if (mfImpactShakeAmount < 0.0f)                                               // fcmpu/bge
        {
            mfImpactShakeAmount  = 0.0f;
            mfNormalShakeAmount += lrSharedInfo.mfTimestep / KF_NORMAL_SHAKE_FADE_TIME;  // +0x3B0
            if (mfNormalShakeAmount > 1.0f)
            {
                mfNormalShakeAmount = 1.0f;
            }
        }

        // [DIAG BRN_DIRECTOR_TRACE] BRING-UP SCAFFOLDING, NOT CONSOLE CODE. Report every
        // meState transition once. This is the measurement this wave exists to make: before it,
        // the state was frozen at E_STATE_PREPARING with no way to observe the fact. Off unless
        // the env var is set; remove with the bring-up path.
        {
            static const bool sbTrace = (getenv("BRN_DIRECTOR_TRACE") != 0);
            static s32 siLastReported = -1;
            if (sbTrace && static_cast<s32>(meState) != siLastReported &&
                CgsDev::Log::gpDebugPrint != 0)
            {
                siLastReported = static_cast<s32>(meState);
                *CgsDev::Log::gpDebugPrint << "[roaming] meState -> " << siLastReported << "\n";
            }
        }

        switch (meState)
        {
        // ---- case 0 @0x822655D0 -------------------------------------------------------
        case E_STATE_INACTIVE:
            break;   // the shared epilogue; the prologue's shake decay has already run

        // ---- case 1 @0x82264488 (12 lines) --------------------------------------------
        // ⚠️ FALLS THROUGH into the DRIVING body in the SAME FRAME -- the console emits no
        // branch at 0x822644B4. This is the whole entry gate: Prepare() is dispatched
        // VIRTUALLY through this->vtable slot 1 (`lwz r11,0(r31); lwz r11,4(r11); bctrl`), not
        // as a direct ArbStateRoaming::Prepare call.
        case E_STATE_PREPARING:
            if (!Prepare(lrSharedInfo))            // 0x8226449C
            {
                break;                             // -> 0x822655D0
            }
            mfStateTimer = 0.0f;                   // stfs f30(0.0), 0x3C0
            meState      = E_STATE_DRIVING;        // stw 2, 0(r21)
            // FALLTHROUGH

        // ---- case 2 @0x822644B8 (123 lines) -- the DRIVING body -----------------------
        // Reaches ProcessPossibleStateChanges @0x8226464C UNCONDITIONALLY: every branch
        // between 0x822644B8 and 0x8226464C is forward-and-inside the arm.
        case E_STATE_DRIVING:
        {
            mMomentSelector.Update(lrSharedInfo.mfTimestep);        // 0x822644C4

            if (!mMomentSelector.HasSelectedMoment())               // lbz +0x398 == selector +0x1E0
            {
                // lwz +0x388 == selector +0x1D0 (muValidMoments), signed `> 0`.
                if (static_cast<s32>(mMomentSelector.GetNumValidMoments()) > 0)
                {
                    // FLAG (type pun, not an offset hack): ArbStateSharedInfo models +0x28 as
                    // `BrnDirector::Random*`, a forward declaration of a type that has no
                    // definition anywhere; MainDirector reinterpret_casts the same bytes into
                    // both that and CgsNumeric::Random (BrnMainDirector.cpp:530 / :946). The
                    // selector's DWARF signature takes CgsNumeric::Random&, so the pun happens
                    // here. De-forking the shared-info slot is its own job.
                    mMomentSelector.SelectBestMoment(
                        *reinterpret_cast<CgsNumeric::Random*>(lrSharedInfo.mpRandom));  // 0x822644E8
                }
            }
            else if (!mMomentSelector.GetSelectedMoment()->IsValid())   // moment +0x174 != 3
            {
                mMomentSelector.CancelSelection();                     // 0x82264510
                mfStateTimer = 0.0f;                                   // 0x82264514
            }

            // ⭐ @0x82264518..0x82264550 -- the two shared gameplay parameter RE-BINDS.
            // RESTORED 2026-08-02 (camera parameter-chain wave), together with the identical
            // pair in SharedCameraContainer::Prepare, exactly as the retired gate's own
            // DELETE-WHEN asked. BehaviourParameterBank now carries the two blocks as real
            // members (see BrnBehaviourParameterBank.h), so these references resolve to the
            // same storage MainDirector::ProcessNewVehicleEvents seeds.
            const Camera::BehaviourParameterBank& lrBank =                 // manager +0x12530
                lrSharedInfo.mpBehaviourManager->GetBehaviourParameterBank();
            lrContainer.mGameplayBumper.GetBehaviour()->SetParameters(
                &lrBank.GetGameplayBumperCameraParamsForCar());            // bank +0x2538
            lrContainer.mGameplayExternal.GetBehaviour()->SetParameters(
                &lrBank.GetGameplayExternalCameraParamsForCar());          // bank +0x2488

            // ---- camera cycle request @0x82264554: the base's per-frame flag, ignored while
            // the lookback override owns the choice; it flips which gameplay cam is selected.
            if (ShouldCycleCameraThisFrame() && !lrContainer.mbLookbackOverride)
            {
                // cntlzw/extrwi == a logical NOT of the byte.
                lrContainer.mbUseGameplayExternal = !lrContainer.mbUseGameplayExternal;
            }

            // ---- this frame's camera source @0x82264580 ------------------------------
            if (mMomentSelector.HasSelectedMoment() &&
                mMomentSelector.GetSelectedMoment()->IsValid())
            {
                // moment +0x10 == the moment's own camera (two separate GetSelectedMoment
                // calls in the asm, 0x82264594 and 0x822645A8; either shape is parity).
                GetNonConstCamera() = mMomentSelector.GetSelectedMoment()->GetCamera();

                // [DIAG] NOT IN THE X360 BINARY. Rung 8 -- THE LAST RUNG of the
                // `[jump-ladder]`: the DRIVING arm has just published a MOMENT's camera
                // instead of GetSelectedGameplayCamera(), i.e. THE CUTAWAY IS ON SCREEN.
                // Every earlier rung can pass and this one still not fire (the moment must
                // still be IsValid() on the frame the camera is chosen), so this is the line
                // that actually witnesses the bug being fixed. One-shot.
                {
                    static bool sbLoggedFirstCutaway = false;
                    if (!sbLoggedFirstCutaway && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbLoggedFirstCutaway = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[FLAG PC bring-up] [jump-ladder] ROAMING DRIVING arm took the"
                               " MOMENT camera -- cutaway live (moment="
                            << mMomentSelector.GetSelectedMoment()->GetName()
                            << " state=" << static_cast<s32>(
                                   mMomentSelector.GetSelectedMoment()->GetState())
                            << ")\n";
                    }
                }
            }
            else if (lrSharedInfo.mpGameState->mDirectorProfileData.maOpaque[0x05] != 0)
            {
                // FLAG: gameState +0x1ED, the same inferred profile-data flag ProcessPossibleFX
                // reads. When set the external chase cam is FORCED (the selection predicate is
                // bypassed entirely -- 0x822645C8 goes straight to container +0x04).
                GetNonConstCamera() = lrContainer.mGameplayExternal.GetProducedCamera();
            }
            else
            {
                GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            }

            ProcessPossibleFX(lrSharedInfo);                 // 0x82264634
            ProcessPossiblePaybackEffects(lrSharedInfo);     // 0x82264640
            ProcessPossibleStateChanges(lrSharedInfo);       // 0x8226464C  ⭐ the transition point

            // If the frame decided to leave DRIVING (and we are not in event mode 3), drop the
            // camera's start-hook request and stop whatever effect is playing.
            if (meState != E_STATE_DRIVING &&
                lrSharedInfo.mpGameState->meEventType != 3)
            {
                GetNonConstCamera().ClearCrashNavEffectGate();                       // stb 0, +0x12F
                Camera::StopCurrentEffect(GetNonConstCamera(), *lrSharedInfo.mpEffectInterface);
                mbStartedJumpEffect = false;                                         // stb 0, +0x39D
            }

            mfStateTimer += lrSharedInfo.mfTimestep;         // 0x82264680..0x8226468C
            break;
        }

        // ==================================================================================
        // ⛔⛔ ARMS 3/4/5/6 -- THE PICTURE-PARADISE / IDLE GROUP. **DEGRADED, DELIBERATELY.**
        //
        // The console bodies (0x822646A4 / 0x82264960 / 0x82264B60 / 0x82264F14, 677 of this
        // function's 1154 instruction lines) drive two BehaviourManager-allocated cameras --
        // a BehaviourRoadRunner and a BehaviourInterpolate -- through
        // NewBehaviour<T> / Setup / IsWaitingToPrepare / HasFailed / HasFinished / Release, plus
        // Camera::EnsureEffectIsPlaying and RequestStartEffectHook on four named hooks
        // ("Black_In_BW", "Black_Out_BW", "Picture_Effect", "Black_In(No_B_W)").
        // They cannot be written here yet because:
        //   * this header carries its OWN private nested BehaviourHandle<T> (a 5-field POD with
        //     no methods) rather than Camera::BehaviourHandle<T> -- the type the manager's
        //     NewBehaviour<T> takes and the only one with GetProducedCamera/GetBehaviour/
        //     IsWaitingToPrepare/Release. De-forking that is its own job;
        //   * BehaviourInterpolate::Setup(f32, BehaviourHelperIndex, BehaviourHelperIndex,
        //     BehaviourManager*) and SharedCameraContainer::GetGameplayCameraHelperIndex have
        //     no bodies anywhere in the tree;
        //   * arms 5 and 6 gate on two 16-byte VMX distance-squared splats (unk_82FAAC00 /
        //     unk_82FAAAC0) that have NOT been decoded. Inventing them would be worse.
        //
        // ⚠️ WHY THIS IS NOT JUST `break;`: ProcessPossibleStateChanges @0x82219FA4 writes
        // meState = 3 DIRECTLY (not through ChangeToStateWithoutRelease), and arm 3 is the ONLY
        // arm of the sixteen that never calls ProcessPossibleStateChanges. An empty arm 3 is a
        // TERMINAL SINK: once a player idles for three minutes anywhere outside a junkyard, the
        // ENTIRE director state machine stops re-evaluating for the rest of the session.
        // What is reproduced instead, using only stores the console itself makes:
        //   * arm 3 takes the console's own "the interpolater could not be produced" exit
        //     (@0x82264828: meState = E_STATE_CHANGING_TO_IDLE_BLACKOUT, mfStateTimer = 0) --
        //     which is exactly this build's situation, since NewBehaviour is never reached;
        //   * arms 4/5/6 keep the camera on GetSelectedGameplayCamera (the console's own
        //     fallback whenever mInterpolater is not allocated -- @0x822646CC), advance
        //     mfStateTimer, and call ProcessPossibleStateChanges, whose 4/5/6 -> DRIVING escape
        //     (ProcessActiveDrivingTransitions, "idle-reset hold") then returns the machine to
        //     DRIVING as soon as the player becomes active again.
        // OBSERVABLE COST: Picture Paradise produces no road-runner fly-around and no
        // black-and-white fades; the game drops back to the normal chase camera instead. That
        // is a visible degradation, NOT a strand, and it is loud rather than silent.
        // DELETE-WHEN: the BehaviourHandle de-fork + BehaviourInterpolate::Setup +
        // GetGameplayCameraHelperIndex land, and the two VMX splats are read.
        // ==================================================================================
        case E_STATE_CHANGING_TO_IDLE_INTERPOLATE:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();   // 0x822646CC arm
            meState      = E_STATE_CHANGING_TO_IDLE_BLACKOUT;                // 0x82264828/0x82264838
            mfStateTimer = 0.0f;                                             // 0x8226491C
            break;

        case E_STATE_CHANGING_TO_IDLE_BLACKOUT:
        case E_STATE_IDLE:
        case E_STATE_IDLE_RESET:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            mfStateTimer += lrSharedInfo.mfTimestep;
            ProcessPossibleStateChanges(lrSharedInfo);   // all three console arms call it too
            break;

        // ==================================================================================
        // The nine CHANGING_TO_* hand-off arms. Eight are the same 31-line shape:
        //     mCamera = lrContainer.GetSelectedGameplayCamera();
        //     ArbUtils::ChangeToStateWithoutRelease<EState>(info, TARGET, meState,
        //                                                   E_STATE_CHANGING_TO_<X>, INACTIVE);
        // ⚠️ Argument 4 is the BLOCKED value and argument 5 the SWITCHED value (see the asm
        // proof in BrnDirectorArbitratorUtils.h). r7 is `li 0` at all nine sites.
        // ==================================================================================

        // ---- case 7 @0x822651B4 (58 lines) -- DEVIATION: an extra CanRun guard + fallback --
        case E_STATE_CHANGING_TO_CRASHING:
        {
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();

            // container +0x1C60 is the CRASHING state, NOT crash-mode: ConstructAll @0x8224F020
            // seeds the pointer table at +0x35A0 with +0x35A8 -> +0x1C60 -> E_STATE_CRASHING.
            ArbitratorState* lpCrashingState =
                lrSharedInfo.mpStateContainer->GetState(ArbitratorStateContainer::E_STATE_CRASHING);

            if (lpCrashingState->CanRun(lrSharedInfo))          // vtable slot 5 (+0x14) @0x8226521C
            {
                ArbUtils::ChangeToStateWithoutRelease<EState>(
                    lrSharedInfo, ArbitratorStateContainer::E_STATE_CRASHING,
                    meState, E_STATE_CHANGING_TO_CRASHING, E_STATE_INACTIVE);
            }
            else
            {
                meState = E_STATE_DRIVING;                     // 0x82265260
                lpCrashingState->Release(lrSharedInfo);        // vtable slot 3 (+0x0C) @0x82265278
                ProcessPossibleStateChanges(lrSharedInfo);     // 0x82265284
            }
            break;
        }

        // ---- case 8 @0x8226529C ------------------------------------------------------
        case E_STATE_CHANGING_TO_TAKEDOWN:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_TAKEDOWN,
                meState, E_STATE_CHANGING_TO_TAKEDOWN, E_STATE_INACTIVE);
            break;

        // ---- case 9 @0x82265318 ------------------------------------------------------
        case E_STATE_CHANGING_TO_RACE_INTRO:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_RACE_INTRO,
                meState, E_STATE_CHANGING_TO_RACE_INTRO, E_STATE_INACTIVE);
            break;

        // ---- case 10 @0x82265394 -----------------------------------------------------
        case E_STATE_CHANGING_TO_ONLINE_RACE_INTRO:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                meState, E_STATE_CHANGING_TO_ONLINE_RACE_INTRO, E_STATE_INACTIVE);
            break;

        // ---- case 11 @0x82265410 -----------------------------------------------------
        case E_STATE_CHANGING_TO_RACE_POST_EVENT:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_POST_EVENT,
                meState, E_STATE_CHANGING_TO_RACE_POST_EVENT, E_STATE_INACTIVE);
            break;

        // ---- case 12 @0x82265138 -----------------------------------------------------
        case E_STATE_CHANGING_TO_DRIVETHRU:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_DRIVETHRU,
                meState, E_STATE_CHANGING_TO_DRIVETHRU, E_STATE_INACTIVE);
            break;

        // ---- case 13 @0x8226548C ⭐ THE JUNKYARD / CAR-SELECT RETRY ARM ---------------
        // MANDATORY. ProcessPossibleStateChanges @0x82219F14 parks meState on 0xD whenever
        // ArbStateCarSelect::Prepare declines (target 8, blocked 0xD, switched 0); this arm is
        // the per-frame re-attempt. Without it the ladder deadlocks on the first decline.
        case E_STATE_CHANGING_TO_CAR_SELECT:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_CAR_SELECT,
                meState, E_STATE_CHANGING_TO_CAR_SELECT, E_STATE_INACTIVE);
            break;

        // ---- case 14 @0x82265508 -----------------------------------------------------
        case E_STATE_CHANGING_TO_RANK_UP:
            GetNonConstCamera() = lrContainer.GetSelectedGameplayCamera();
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_RANK_UP,
                meState, E_STATE_CHANGING_TO_RANK_UP, E_STATE_INACTIVE);
            break;

        // ---- case 15 @0x82265584 (11 lines) -- DEVIATION: NO camera copy --------------
        case E_STATE_CHANGING_TO_ONLINE_CAR_SELECT:
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_CAR_SELECT,
                meState, E_STATE_CHANGING_TO_ONLINE_CAR_SELECT, E_STATE_INACTIVE);
            break;

        // ---- default @0x822655B0 -- reached by E_STATE_RELEASING (16) and anything above.
        default:
            CGS_ASSERT(false, "unhandled state");   // BrnArbStateRoaming.cpp:643 (0x283)
            break;
        }
    }

    // ------------------------------------------------------------------------
    // ProcessPossiblePaybackEffects @0x82208BA8 -- ⭐ NEW 2026-08-01.
    //
    // SIGNATURE FROM ASM: `mr r23, r4` / `mr r22, r3`, f1 never read, epilogue a bare
    // `b __restgprlr_22` -- void(ArbStateSharedInfo&). Hex-Rays' `float*(float*, int)` is noise.
    //
    // Called UNCONDITIONALLY from the DRIVING arm (0x82208BA8 <- 0x82264640), so it is on the
    // junkyard path by construction. It never writes meState, never touches a behaviour handle
    // and never calls ChangeTo* -- it only keeps the camera's effect-hook request in sync with
    // the game state's active payback.
    // ------------------------------------------------------------------------
    void ArbStateRoaming::ProcessPossiblePaybackEffects(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState&             lrGameState = *lrSharedInfo.mpGameState;
        const EffectInterface& lrEffects   = *lrSharedInfo.mpEffectInterface;
        Camera::Camera&        lrCamera    = GetNonConstCamera();

        // off_82001AC0 -- a 3-entry const char* table (it ends at 0x82001ACC, which is the loop
        // bound the console compares r30 against at 0x82208D54).
        static const char* const KAPC_PAYBACK_EFFECT_NAMES[3] =
        {
            "Reverse",       // E_PAYBACK_TYPE_REVERSE_STEERING (0)
            "Boost_Lock",    // E_PAYBACK_TYPE_BOOST_LOCK       (1)
            "Boost_Steal"    // E_PAYBACK_TYPE_AGGRESSORS_CONTROLS_AFFECTS_VICTIM (2)
        };

        if (lrGameState.mbPaybackActive)                        // lbz +0xF0
        {
            // Request the payback hook unless it is ALREADY the live effect.
            bool lbNeedsRequest = true;
            if (lrEffects.HasCurrentEffectName())               // lbz +0xD37
            {
                const char* lpcWanted =
                    KAPC_PAYBACK_EFFECT_NAMES[lrGameState.meActivePaybackType];   // lwz +0xF4
                if (strcmp(lrEffects.GetCurrentEffectName(), lpcWanted) == 0)     // 0x82208BFC loop
                {
                    lbNeedsRequest = false;                    // 0x82208C24 -> the tail
                }
            }

            if (lbNeedsRequest)
            {
                // :1094 -- the table only covers types 0..2; type 3 (SIX_AXIS_STEERING) has no
                // camera effect. Non-gating in the console: it indexes the table either way.
                CGS_ASSERT(lrGameState.meActivePaybackType != BrnNetwork::E_PAYBACK_TYPE_SIX_AXIS_STEERING,
                           "lSharedInfo.mpGameState->meActivePayback");
                Camera::RequestStartEffectHook(
                    lrCamera, KAPC_PAYBACK_EFFECT_NAMES[lrGameState.meActivePaybackType], KF_UNIT);
            }
        }

        // 0x82208C88 -- the complement: payback is NOT active but something IS playing. Walk
        // the three names and stop the effect on the first match.
        if (!lrGameState.mbPaybackActive && lrEffects.HasCurrentEffectName())
        {
            for (s32 liEnumIndex = 0; liEnumIndex < 3; ++liEnumIndex)
            {
                // The console re-asserts this every iteration (EffectTrigger.h:112, the
                // GetCurrentEffectName tripwire re-emitted inline at 0x82208CD4).
                CGS_ASSERT(lrEffects.HasCurrentEffectName(), "HasCurrentEffectName()");

                if (strcmp(lrEffects.GetCurrentEffectName(),
                           KAPC_PAYBACK_EFFECT_NAMES[liEnumIndex]) == 0)
                {
                    Camera::StopCurrentEffect(lrCamera, lrEffects);   // 0x82208D70
                    return;
                }

                // The inlined GetPaybackTypeName helper's own bound check
                // (GameSource/Network/SharedIO/..., line 182). ⚠️ FLAG: the console compares
                // `cmpwi r25, 3` while this tree's BrnNetworkSharedIO.h carries
                // E_PAYBACK_TYPE_COUNT == 4. The asm's number is kept (it can never fire from
                // this 3-iteration loop either way); the enum discrepancy is not resolved here.
                CGS_ASSERT((liEnumIndex + 1) <= 3, "leEnumIndex <= E_PAYBACK_TYPE_COUNT");
            }
        }
    }

    // ------------------------------------------------------------------------
    // ProcessPossibleFX @0x82234A00 -- per-frame, request whichever camera-PFX hook the current
    // game event wants (or stop the current effect), then apply the impact-shake state.
    //
    // The switch selects on the game event mode (mpGameState->meEventType, console +0x120).
    // FLAG: several GameState sub-fields below are read at console offsets whose named member is
    // inferred from the GameState anchors + the FX semantics; each such read is FLAGged.
    // ------------------------------------------------------------------------
    void ArbStateRoaming::ProcessPossibleFX(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState&             lrGameState = *lrSharedInfo.mpGameState;
        const EffectInterface& lrEffects   = *lrSharedInfo.mpEffectInterface;
        Camera::Camera&        lrCamera    = GetNonConstCamera();

        // X360 clears the "Race_Day effect active" dirty bit at entry; the crash-mode case
        // re-sets it below.
        lrCamera.mState_uFlags &= ~static_cast<s32>(KU_DIRTY_RACE_DAY_ACTIVE);

        switch (lrGameState.meEventType)
        {
        case -1:    // free roam (no event): establishing-shot / smash / billboard path
        {
            // Skip if the selected moment is already a valid establishing shot.
            if (mMomentSelector.HasSelectedMoment() &&
                mMomentSelector.GetSelectedMoment()->GetState() == Moment::E_STATE_VALID)
            {
                break;
            }

            Camera::StopCurrentEffect(lrCamera, lrEffects);

            if ((lrGameState.miThisFramesActionFlags & 0x10) != 0)   // smash action this frame
            {
                Camera::RequestStartEffectHook(lrCamera, "Smash_Effect", KF_UNIT);
            }
            if ((lrGameState.miThisFramesActionFlags & 0x04) != 0)   // billboard action this frame
            {
                Camera::RequestStartEffectHook(lrCamera, "Billboard_Effect", KF_UNIT);
                mbStartedJumpEffect = true;     // +0x39D
            }

            // Showtime-intro borders/blur ramp, gated on a DirectorProfileData flag.
            // FLAG: console +0x1ED -> mDirectorProfileData (showtime-intro active); the sub-
            // object is opaque, so the flag is read through its byte blob.
            const bool lbShowtimeIntroActive =
                lrGameState.mDirectorProfileData.maOpaque[0x05] != 0;  // FLAG: +0x1ED inferred

            if (lbShowtimeIntroActive)
            {
                mfTimeInShowtimeIntro += lrSharedInfo.mfTimestep;      // +0x3A4 += dt

                // Latch the borders duration on first leaving the FP dead-zone.
                const bool lbBordersInDeadZone =
                    !(mfBordersTime > KF_FP_EPSILON || mfBordersTime < -KF_FP_EPSILON);
                if (lbBordersInDeadZone)
                {
                    mfBordersTime = KF_SHOWTIME_BORDERS_DUR;
                }

                // Blur and black-bars amounts, clamped to [0, max].
                const f32 lfBlurRaw     = mfTimeInShowtimeIntro * KF_SHOWTIME_BLUR_RAMP;
                const f32 lfBlurClamped = (lfBlurRaw < KF_SHOWTIME_BLUR_MAX) ? lfBlurRaw : KF_SHOWTIME_BLUR_MAX;
                const f32 lfBlur = (lfBlurClamped > 0.0f) ? lfBlurClamped : 0.0f;
                const f32 lfBarsRaw  = KF_UNIT - lfBlurClamped;
                const f32 lfBars = (lfBarsRaw > 0.0f) ? lfBarsRaw : 0.0f;
                lrCamera.SetShowtimeBlurAndBars(lfBlur, lfBars, /*lbHasStart*/ true, /*lbHasStop*/ true);
            }
            else
            {
                mfTimeInShowtimeIntro = 0.0f;
            }

            // Borders fade-out: once out of the dead-zone, drive the post-FX blur and decay the
            // borders timer toward zero.
            const bool lbBordersActive =
                (mfBordersTime > KF_FP_EPSILON || mfBordersTime < -KF_FP_EPSILON);
            if (lbBordersActive)
            {
                const f32 lfPostFX = (KF_UNIT - (mfBordersTime / KF_SHOWTIME_BORDERS_DUR)) * KF_SHOWTIME_POSTFX_SCALE;
                lrCamera.SetRequestedPostFX(lfPostFX);
                const f32 lfDecayed = mfBordersTime - lrSharedInfo.mfSimTimestep;
                mfBordersTime = (lfDecayed > 0.0f) ? lfDecayed : 0.0f;
            }
            break;
        }

        case 0:     // crash mode: play Race_Day while the event state is ACTIVE
        {
            if (lrGameState.mEventState.GetCurrent() != GameState::E_EVENT_STATE_ACTIVE)
            {
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                break;
            }
            const f32 lfRaceDayBlend = lrSharedInfo.mpPlayerTracker->GetRaceEndEffectAmount();
            if (lfRaceDayBlend <= 0.0f)
            {
                Camera::StopCurrentEffect(lrCamera, lrEffects);
                break;
            }
            if (mbPlayRaceEndEffect)    // +0x3A0 (this state's own "play race end effect" flag)
            {
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Race_Day", lfRaceDayBlend);
            }
            else
            {
                Camera::StopCurrentEffect(lrCamera, lrEffects);
            }
            lrCamera.mState_uFlags |= static_cast<s32>(KU_DIRTY_RACE_DAY_ACTIVE);
            break;
        }

        case 3:     // takedown / damage-crit family
        case 8:
        {
            // asm 0x82234B1C: lbz +0x1C0 -> mbRoadRageOneMoreCrashToWrecked (the "damage is
            // critical" gate, per the Clear @0x82218930 map; read just before +0x1BC below).
            if (lrGameState.mbRoadRageOneMoreCrashToWrecked)   // +0x1C0
            {
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Damage_Crit",
                                              KF_DAMAGE_CRIT_BASE_BLEND);
            }
            else
            {
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Damage_Crit",
                                              lrGameState.mfHowCloseToTotalled * KF_DAMAGE_CRIT_BASE_BLEND);
            }
            break;
        }

        case 7:     // smash event
        {
            Camera::StopCurrentEffect(lrCamera, lrEffects);
            // asm 0x82234B7C: lbz +0x1B5 -> mbPlayerPerformedStunt (the smash-event gate).
            if (lrGameState.mbPlayerPerformedStunt)            // +0x1B5
            {
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Smash_Effect", KF_UNIT);
                // asm 0x82234BA4: stfs f0, 0x3AC(r31) -> mfImpactShakeAmount (pseudocode *(a1+940)=1.0,
                // 940=0x3AC); the smash event pre-loads mfImpactShakeAmount so the tail's
                // if(mfImpactShakeAmount>0) SetImpactShake path fires. (+0x3AC, NOT +0x3B0 mfNormalShakeAmount.)
                mfImpactShakeAmount = KF_UNIT;   // +0x3AC
                mu8ImpactShakeType  = 8;         // +0x3C4
            }
            break;
        }

        case 10:    // race-intro family: stop FX, then checkpoint hook if requested
        case 11:
        case 15:
        case 16:
        {
            Camera::StopCurrentEffect(lrCamera, lrEffects);
            // asm 0x82234BC0: lbz +0x1B4 -> mbPlayerHitCheckpointThisFrame (checkpoint hook).
            if (lrGameState.mbPlayerHitCheckpointThisFrame)    // +0x1B4
            {
                Camera::RequestStartEffectHook(lrCamera, "Checkpoint", KF_UNIT);
            }
            break;
        }

        case 12:    // post-event / drive-thru / online-car-select: road-rage wrecked logic
        case 14:
        case 17:
        {
            // asm 0x82234C8C: lbz +0x1D0 -> road-rage "critical" bool (RankUpInfo blob, opaque[0]).
            if (lrGameState.mRankUpInfo.maOpaque[0x00] != 0)   // FLAG: +0x1D0 (opaque road-rage bool)
            {
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Damage_Crit", KF_UNIT);
                break;
            }
            // asm 0x82234CB4: lbz +0x1D1 -> road-rage "totalled" bool (RankUpInfo blob, opaque[1]).
            if (lrGameState.mRankUpInfo.maOpaque[0x01] != 0)   // FLAG: +0x1D1 (opaque road-rage bool)
            {
                // asm 0x82234CC0: lwz +0x1CC (full 4-byte word) -> the rival-team selector passed
                // to the opposing-team query.
                const s32 liRivalTeam = lrGameState.mRankUpInfo.miRivalTeamSelector;  // +0x1CC (word)
                const f32 lfSqDist =
                    lrSharedInfo.mpAllVehicleData->SqDistanceOfNearestOpposingTeamMember(liRivalTeam);
                if (lfSqDist < KF_NEAREST_CAR_FX_RANGE_SQ)
                {
                    const f32 lfWreck = KF_UNIT - mfWreckedEffectAmount;
                    Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Wrecked", lfWreck * lfWreck);
                    const f32 lfNext = mfWreckedEffectAmount - lrSharedInfo.mfTimestep;
                    mfWreckedEffectAmount = lfNext;
                    if (lfNext >= 0.0f)
                    {
                        break;
                    }
                }
                mfWreckedEffectAmount = KF_UNIT;
            }
            break;
        }

        case 13:    // drive-thru: damage-crit fade by player distance + checkpoint hook
        {
            // asm 0x82234BF4: lwz +0x1CC (full 4-byte word) compared == 2 (rival-team selector).
            if (lrGameState.mRankUpInfo.miRivalTeamSelector == 2)   // +0x1CC (word)
            {
                const f32 lfPlayerSqDist =
                    lrSharedInfo.mpAllVehicleData->GetSqDistanceOfNearestCarToPlayer();
                // Clamp the distance up to >= the inner range, then map [inner..outer] -> [0..1].
                const f32 lfClamped = ((KF_NEAREST_CAR_FX_RANGE_SQ - lfPlayerSqDist) > 0.0f)
                                          ? KF_NEAREST_CAR_FX_RANGE_SQ : lfPlayerSqDist;
                const f32 lfBlend =
                    -(((lfClamped - KF_NEAREST_CAR_FX_RANGE_SQ) * KF_NEAREST_CAR_FX_FALLOFF) - KF_UNIT);
                Camera::EnsureEffectIsPlaying(lrCamera, lrEffects, "Damage_Crit", lfBlend);
            }
            // asm 0x82234C58: lbz +0x1B4 -> mbPlayerHitCheckpointThisFrame (as in cases 10..16).
            if (lrGameState.mbPlayerHitCheckpointThisFrame)    // +0x1B4
            {
                Camera::RequestStartEffectHook(lrCamera, "Checkpoint", KF_UNIT);
            }
            break;
        }

        default:    // every other event mode just stops the current effect
            Camera::StopCurrentEffect(lrCamera, lrEffects);
            break;
        }

        // ---- impact-shake / landed-stunt application (the X360 tail) ----------------------
        // asm 0x82234EEC/EF8/F10 read three adjacent GameState bools (per the Clear @0x82218930
        // offset map): +0x1B1 mbPlayerAndRivalImpactOccured, +0x1B2 mbPlayerWonImpactAgainstRival,
        // +0x1B3 mbPlayerCheckedTraffic.
        if (lrGameState.mbPlayerAndRivalImpactOccured)         // +0x1B1
        {
            mu8ImpactShakeType = 8;                            // stb r26(=8), +0x3C4
            // asm 0x82234EF8 loads +0x1B2 (mbPlayerWonImpactAgainstRival) and compares it, but the
            // branch at 0x82234F0C is unconditional -- the compare result is discarded, so this is
            // a dead read: when +0x1B1 is set the shake amount is always the landed-stunt flash.
            (void)lrGameState.mbPlayerWonImpactAgainstRival;   // +0x1B2 (dead load in the asm)
            mfImpactShakeAmount = KF_STUNT_LANDED_FLASH;       // +0x3AC = 1.25 (flt_820092CC)
        }
        else if (lrGameState.mbPlayerCheckedTraffic)           // +0x1B3
        {
            mu8ImpactShakeType  = 9;                           // stb 9, +0x3C4
            mfImpactShakeAmount = KF_UNIT;                     // +0x3AC = 1.0
        }
        // else: leave mfImpactShakeAmount untouched (asm falls through 0x82234F10 -> 0x82234F2C).

        // Apply the shake into the camera's effects block.
        if (mfImpactShakeAmount > 0.0f)
        {
            lrCamera.SetImpactShake(mfImpactShakeAmount, KF_UNIT, mu8ImpactShakeType);
        }
        else
        {
            // Non-positive amount: decay the live shake amplitude by it.
            lrCamera.ScaleImpactShake(mfImpactShakeAmount);
        }
    }

    // ------------------------------------------------------------------------
    // ProcessPossibleStateChanges @0x82219C58 -- decide whether the roaming state should hand
    // off to another arbitrator state this frame, based on the game-state snapshot. Each
    // transition routes through ArbUtils::ChangeToStateWithoutRelease<EState>.
    // ------------------------------------------------------------------------
    void ArbStateRoaming::ProcessPossibleStateChanges(ArbStateSharedInfo& lrSharedInfo)
    {
        GameState&                lrGameState = *lrSharedInfo.mpGameState;
        ArbitratorStateContainer& lrContainer = *lrSharedInfo.mpStateContainer;

        // The X360 transition helper always sets meState to INACTIVE on a successful switch
        // (a5 == 0) and to the CHANGING_TO_<dest> value when the destination state declines
        // (a4). E_STATE_INACTIVE is the shared "switched" value below.

        // The crash-mode early path: crash is active, the camera is not already crash-flagged,
        // the takedown action bit is clear, and the crash-mode state says it can run.
        const bool lbCrashFlagSet = (GetCamera().mState_uFlags & static_cast<s32>(KU_DIRTY_CRASH_FLAG)) != 0;
        const bool lbTakedownBit  = (lrGameState.miThisFramesActionFlags & 0x20) != 0;

        // FLAG: console +0xF9 -> mbCrashActive (Clear stores 0 here; this gate enters crash mode).
        const bool lbCrashModeCandidate =
            lrGameState.mbCrashActive && !lbCrashFlagSet && !lbTakedownBit &&
            lrContainer.GetState(ArbitratorStateContainer::E_STATE_CRASH_MODE)->CanRun(lrSharedInfo);

        // [diag] BRN_CRASHCAM_DIAG -- NOT IN THE X360 BINARY. The crash edge has FOUR
        // conjuncts and three of them are silent; printing the whole tuple once per change is
        // the difference between "the arbitrator refused" and "the arbitrator was never asked".
        if (getenv("BRN_CRASHCAM_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siLast = -1;
            const s32 liNow = (lrGameState.mbCrashActive ? 1 : 0)
                            | (lbCrashFlagSet           ? 2 : 0)
                            | (lbTakedownBit            ? 4 : 0)
                            | (lbCrashModeCandidate     ? 8 : 0)
                            | (static_cast<s32>(meState) << 8);
            if (liNow != siLast)
            {
                siLast = liNow;
                *CgsDev::Log::gpDebugPrint
                    << "[crashcam] roaming crash edge: crashActive="
                    << (lrGameState.mbCrashActive ? 1 : 0)
                    << " camCrashFlag=" << (lbCrashFlagSet ? 1 : 0)
                    << " takedownBit=" << (lbTakedownBit ? 1 : 0)
                    << " candidate=" << (lbCrashModeCandidate ? 1 : 0)
                    << " roamingState=" << static_cast<s32>(meState) << "\n";
            }
        }

        if (lbCrashModeCandidate)
        {
            // asm @0x82219CF4: target container EState 2 (CRASHING), blocked-value 7.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_CRASHING,
                meState, E_STATE_CHANGING_TO_CRASHING, E_STATE_INACTIVE);
        }
        else if (lrGameState.mbTakedownActive)   // +0xDA
        {
            // asm @0x82219D20: target 3 (TAKEDOWN), blocked 8.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_TAKEDOWN,
                meState, E_STATE_CHANGING_TO_TAKEDOWN, E_STATE_INACTIVE);
        }
        else if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_PRE_INTRO)
        {
            // No event yet (event-state journal head == PRE_INTRO). Only these event modes
            // hand off to the race-intro state; every other mode does nothing this frame.
            switch (lrGameState.meEventType)
            {
            case 0: case 2: case 3: case 5: case 7: case 8:
                // asm @0x82219DC0: target 6 (RACE_INTRO), blocked 9.
                ArbUtils::ChangeToStateWithoutRelease<EState>(
                    lrSharedInfo, ArbitratorStateContainer::E_STATE_RACE_INTRO,
                    meState, E_STATE_CHANGING_TO_RACE_INTRO, E_STATE_INACTIVE);
                break;
            default:
                break;
            }
        }
        else
        {
            // An event is running. The X360 first asks whether we are sitting in a steady
            // ACTIVE event (previous-frame event state was PRE_INTRO AND the current state is
            // ACTIVE): in that case it hands the frame to the running race state via the
            // container. Otherwise it walks the intro / post-event / driving transition ladder.
            const bool lbSteadyActiveEvent =
                lrGameState.mEventState.GetPrevious(0) == GameState::E_EVENT_STATE_PRE_INTRO &&
                lrGameState.mEventState.GetCurrent()   == GameState::E_EVENT_STATE_ACTIVE;

            if (lbSteadyActiveEvent)
            {
                // Hand off to the running race state (the X360 dispatches a virtual through the
                // container's embedded race-intro / online-race-intro sub-state).
                // FLAG: the container sub-state offsets (+0x2B80 / +0x29D0) are mapped to the
                // race-intro EStates by role; the vtable slot called (+0xC) is Release.
                const s32 liEventMode = lrGameState.meEventType;
                const bool lbOnlineRaceIntro =
                    liEventMode >= 10 && (liEventMode <= 14 || liEventMode == 17);
                ArbitratorStateContainer::EState leTarget =
                    lbOnlineRaceIntro ? ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO
                                      : ArbitratorStateContainer::E_STATE_RACE_INTRO;
                lrContainer.GetState(leTarget)->Release(lrSharedInfo);
            }
            else
            {
                ProcessActiveDrivingTransitions(lrSharedInfo, lrGameState);
            }
        }

        // After any transition decision: if we did NOT settle on DRIVING and a moment is still
        // selected, cancel the moment selection.
        if (meState != E_STATE_DRIVING && mMomentSelector.HasSelectedMoment())
        {
            mMomentSelector.CancelSelection();
        }
    }

    // The non-steady-event transition ladder (X360 inner block of ProcessPossibleStateChanges,
    // flattened inline there). Reached when an event is running but it is not a steady ACTIVE
    // event: react to the intro / post-event lifecycle, then to drive-thru / car-select /
    // showtime / idle-reset / rank-up / online-car-select. Several GameState reads are offset-
    // inferred (FLAGged).
    void ArbStateRoaming::ProcessActiveDrivingTransitions(ArbStateSharedInfo& lrSharedInfo,
                                                          GameState& lrGameState)
    {
        const s32 liEventMode = lrGameState.meEventType;
        const bool lbRaceIntroMode = liEventMode >= 10 && (liEventMode <= 14 || liEventMode == 17);

        if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_INTRO)
        {
            if (lbRaceIntroMode)
            {
                // asm @0x82219EA4: target 7 (ONLINE_RACE_INTRO), blocked 0xA.
                ArbUtils::ChangeToStateWithoutRelease<EState>(
                    lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_RACE_INTRO,
                    meState, E_STATE_CHANGING_TO_ONLINE_RACE_INTRO, E_STATE_INACTIVE);
            }
            else
            {
                // asm @0x82219DC0 (shared with the PRE_INTRO arm): target 6 (RACE_INTRO),
                // blocked 9.
                ArbUtils::ChangeToStateWithoutRelease<EState>(
                    lrSharedInfo, ArbitratorStateContainer::E_STATE_RACE_INTRO,
                    meState, E_STATE_CHANGING_TO_RACE_INTRO, E_STATE_INACTIVE);
            }
            return;
        }
        if (lrGameState.mEventState.GetCurrent() == GameState::E_EVENT_STATE_POST_EVENT)
        {
            // asm @0x82219ED4: target 5 (POST_EVENT), blocked 0xB.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_POST_EVENT,
                meState, E_STATE_CHANGING_TO_RACE_POST_EVENT, E_STATE_INACTIVE);
            return;
        }
        if (lrGameState.mbDriveThruActive)       // +0xD0
        {
            // asm @0x82219F00: target 0 (DRIVETHRU), blocked 0xC.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_DRIVETHRU,
                meState, E_STATE_CHANGING_TO_DRIVETHRU, E_STATE_INACTIVE);
            return;
        }
        if (lrGameState.meJunkyardState != GameState::E_JY_INACTIVE)   // +0x180
        {
            // asm @0x82219F28: target 8 (CAR_SELECT), blocked 0xD.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_CAR_SELECT,
                meState, E_STATE_CHANGING_TO_CAR_SELECT, E_STATE_INACTIVE);
            return;
        }

        // Showtime (Picture Paradise) entry: the player has been inactive long enough, is far
        // enough out, the state is DRIVING, the player has been active, and picture-paradise is
        // not disabled. Offsets confirmed against the Clear @0x82218930 map: +0x14C
        // mfPlayerInactiveTime, +0x148 mfPadInactiveTime, +0x150 mbPlayerBeenActive, +0x100
        // mbCanUseSlomo, +0x103 mbShortCutMenuShown.
        const bool lbShowtimeEntry =
            lrGameState.mfPlayerInactiveTime > KF_IDLE_TIME_BEFORE_PICTURE_PARADISE &&  // +0x14C (asm: > flt_82CDA494)
            lrGameState.mfPadInactiveTime > 13.0f &&            // +0x148 (asm: > flt_820019D4)
            meState == E_STATE_DRIVING &&
            lrGameState.mbPlayerBeenActive &&                   // +0x150
            lrGameState.mbCanUseSlomo &&                        // +0x100
            !lrGameState.mbShortCutMenuShown &&                 // +0x103
            !mbDisablePictureParadise;                          // +0x39F

        if (lbShowtimeEntry)
        {
            mfStateTimer = 0.0f;             // +0x3C0 = 0.0
            meState      = E_STATE_CHANGING_TO_IDLE_INTERPOLATE;   // +0x3BC = 3
            return;
        }

        // Idle-reset hold: while in one of the idle states, drop back to DRIVING once the player
        // becomes active again (inactive-time hits zero, or slomo unavailable, or pic-paradise
        // disabled).
        if ((meState == E_STATE_CHANGING_TO_IDLE_BLACKOUT ||
             meState == E_STATE_IDLE ||
             meState == E_STATE_IDLE_RESET) &&
            (lrGameState.mfPlayerInactiveTime == 0.0f || !lrGameState.mbCanUseSlomo ||
             mbDisablePictureParadise))
        {
            meState = E_STATE_DRIVING;       // +0x3BC = 2
            return;
        }

        // Rank-up / online-car-select hand-offs.
        // asm 0x82219FF4: lbz +0x1D4 -> rank-up active (lands in the opaque mShowTimeInfo blob).
        // asm 0x8221A018: lbz +0x1A5 -> mbIsOnlineCarSelectActive (Clear default 0).
        if (lrGameState.mShowTimeInfo.maOpaque[0x00] != 0)      // FLAG: +0x1D4 (opaque rank-up bool)
        {
            // asm @0x8221A010: target 9 (RANK_UP), blocked 0xE.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_RANK_UP,
                meState, E_STATE_CHANGING_TO_RANK_UP, E_STATE_INACTIVE);
            return;
        }
        if (lrGameState.mbIsOnlineCarSelectActive)             // +0x1A5
        {
            // asm @0x8221A034: target 0xA (ONLINE_CAR_SELECT), blocked 0xF.
            ArbUtils::ChangeToStateWithoutRelease<EState>(
                lrSharedInfo, ArbitratorStateContainer::E_STATE_ONLINE_CAR_SELECT,
                meState, E_STATE_CHANGING_TO_ONLINE_CAR_SELECT, E_STATE_INACTIVE);
            return;
        }
    }

    // ------------------------------------------------------------------------
    // Release @0x82234930 -- leave the roaming state: drop the moment selection, release the two
    // camera behaviours back to the manager, and assert no behaviours remain allocated.
    // ------------------------------------------------------------------------
    bool ArbStateRoaming::Release(ArbStateSharedInfo& lrSharedInfo)
    {
        if (mMomentSelector.HasSelectedMoment())
        {
            mMomentSelector.CancelSelection();
        }
        mMomentSelector.Release();

        if (mInterpolater.mbAllocated)          // +0x180 block
        {
            mInterpolater.mpManager->UnSetBehaviourUsedByHandle(mInterpolater.muAllocationKey);
            mInterpolater.muHelperIndex = 0;
            mInterpolater.mpManager     = 0;
            mInterpolater.mpBehaviour   = 0;
            mInterpolater.mbAllocated   = false;
        }

        if (mRoadRunnerCam.mbAllocated)         // +0x194 block
        {
            mRoadRunnerCam.mpManager->UnSetBehaviourUsedByHandle(mRoadRunnerCam.muAllocationKey);
            mRoadRunnerCam.muHelperIndex = 0;
            mRoadRunnerCam.mpManager     = 0;
            mRoadRunnerCam.mpBehaviour   = 0;
            mRoadRunnerCam.mbAllocated   = false;
        }

        meState = E_STATE_INACTIVE;             // +0x3BC = 0

        lrSharedInfo.mpBehaviourManager->CheckNoBehavioursAreAllocatedByState(this);
        return true;
    }
}
