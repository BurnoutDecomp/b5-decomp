#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::IsUsingMetricUnits
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"               // MovieClipInstance::ResetTimeline

// includes folded in from the BrnEventInfo_w*.cpp partfiles (2026-09-15)
#include "BrnCommonTypes.h"                                                        // Vector2, Vector4
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                             // MovieClipRef::{GotoAndPlayLabel,FindChildMovieClipOnFrame,SetFrameTriggerCallback}
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // TextFieldRef::{SetText,SetAutoSize,SetColour}
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"  // TryFindTextFieldFromMovieClip
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"
#include <cmath>                                                  // std::floor (the inlined BrnMath::IntRound)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // the [event-info] deferred-arm log
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SPrintf / CgsCore::SnPrintf
#include "GameSource/BurnoutConstants.h"                           // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameSource/Gui/BrnGuiCache.h"                            // BrnGui::GuiCache (+ StuntToDisplayInfo)
#include "GameSource/GameState/BrnGameStateTypes.h"                // BrnGameState::EStuntType
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData

// ============================================================================
// BrnGui::EventInfoComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here so far:
//   Construct                @0x82421160
//   Prepare                  @0x82412C00
//   ClearEventSpecificData   @0x82412CA8
//   SetPositionTextState     @0x8241EDF8
//   SetTakedownsDigitsState  @0x824218A8
//
// The remaining 21 ledger functions (MoveAnimation @0x82429570,
// PrepareComponentsForGameMode @0x82429150, SetEventType @0x8242FC78,
// Update @0x82435430, UpdateStuntAttack @0x82429C08, SetTextFieldDangerColour
// @0x82421980, BankingTransitionCompleteCallback @0x824214C8 and the per-mode
// Update* arms) are declared in the header and belong in this same TU.
//
// NOTE FOR WHOEVER MOUNTS THIS TU: BrnFlapt::TextFieldRef::GetText @0x8246E0C0 is
// DECLARATION-ONLY in the tree and UpdateStuntAttack calls it twice -- bodying that
// leaf is a prerequisite for adding this file to tools/build/build_game_exe.bat.
// ============================================================================

namespace BrnGui
{

namespace
{
    // Construct's tail (X360 flt_820DB5B4 / flt_820DB5AC, read out of the image
    // 2026-08-27). DWARF names the metric one KF_DISTANCE_WARNING_THRESHOLD
    // (BrnEventInfo.h:198); the imperial sibling is the same distance in metres.
    const f32 KF_DISTANCE_WARNING_THRESHOLD_METRIC   = 1000.0f;      // 0x447A0000
    const f32 KF_DISTANCE_WARNING_THRESHOLD_IMPERIAL = 1609.344f;    // 0x44C92B02 (one mile)

    // ClearEventSpecificData's landmark sentinel. The X360 loads the 16-bit constant
    // word_82F25440, which the image holds as 0xFFFF -- the invalid LandmarkIndex.
    const s32 KI_INVALID_LANDMARK = -1;                              // word_82F25440 == 0xFFFF

    // ClearEventSpecificData seeds the medal target with 3 == BrnGameState::
    // E_CURRENT_MEDAL_TARGET_TIME_NONE (BrnScoringSystem.h:150). The enum is only
    // forward-declared (opaque, `: s32`) in this component's include set -- pulling
    // BrnScoringSystem.h in here would drag the whole scoring keystone into a HUD TU --
    // so the attested integer is cast at the store, exactly as BrnGuiCache.h:1327 does.
    const s32 KI_CURRENT_MEDAL_TARGET_TIME_NONE = 3;

    // ClearEventSpecificData seeds meSecurity with 3. BrnNetwork::EBrnGameSecurity runs
    // 0 public / 1 private / 2 closed (BrnGuiCache.h:1444), so 3 is the count/invalid
    // slot; no home header declares the enum yet, hence the plain integer.
    const s32 KI_BRN_GAME_SECURITY_INVALID = 3;
}

// ---------------------------------------------------------------------------
// @0x82421160 (decl BrnEventInfo.h:78) -- construct the component: adopt the state
// channel, clear the nine text-field handles, construct the thirteen animators and
// the distance interpolator, reset the per-event scalars, and latch the metric or
// imperial distance-warning threshold.
//
// The X360 body ignores lacName and lacParentName entirely (they exist for the shared
// component signature) and, notably, does NOT touch mEventMovieClip -- the console
// leaves that handle alone until MoveAnimation @0x82429570 re-binds it out of the
// mode frame. Reproduced faithfully.
//
// Call site: RaceMainHudState::OnEnter @0x82478EF8 (asm @0x824790F8) --
// Construct(this + 0x170, "EventInfo_mc", mpStateInterface, NULL).
// ---------------------------------------------------------------------------
void EventInfoComponent::Construct(const char* lacName,
                                   CgsGui::StateInterface* lpStateInterface,
                                   const char* lacParentName)
{
    (void)lacName;
    (void)lacParentName;

    // Base construct, inlined by the X360 (the h:113 tripwire, the state-channel store
    // at +0x00 and the mAptRef clear at +0x04/+0x08). Restored as the real call.
    BrnFlaptComponent::Construct(lpStateInterface);

    meCurrentEventType       = BrnGameState::GameStateModuleIO::E_MODE_NONE;   // stw -1, 0x3D4
    meCurrentOnlineGameMode  = BrnGameState::GameStateModuleIO::E_MODE_NONE;   // stw -1, 0x3D8
    miCurrentEventStateIndex = 0;                                             // stw  0, 0x468

    // The nine TextFieldRefs: the X360 zeroes +0x1C..+0x84 in one 27-dword run, which
    // is exactly maTextField[7] + mAddScoreTextField + mBankScoreTextField.
    for (s32 liTextField = 0; liTextField < KI_TEXTFIELD_COUNT; ++liTextField)
    {
        maTextField[liTextField].SetInvalid();
    }
    mAddScoreTextField.SetInvalid();
    mBankScoreTextField.SetInvalid();

    // The thirteen animators, in X360 construction order (each a vtbl[0] virtual call
    // with the literal DEBUG name, the component's own state channel and a NULL parent).
    mAddScoreAnimator.Construct("AddScore_anim", mpStateInterface, 0);
    mScoreBackgroundAnimator.Construct("ScoreBacking_anim", mpStateInterface, 0);
    mTextStateAnimatorRace.Construct("raceTextState_anim", mpStateInterface, 0);
    mTextStateAnimatorRRage.Construct("rRageTextState_anim", mpStateInterface, 0);
    mTakedownNumbersAnimator.Construct("TakedownNumbersAnimator", mpStateInterface, 0);
    mDistanceAnimatorRace.Construct("raceDistance_anim", mpStateInterface, 0);
    mTimeAnimatorRRage.Construct("CurrentTimeAnimRRage_cpt", mpStateInterface, 0);
    mTimeAnimatorBRoute.Construct("CurrentTimeAnimBRoute_cpt", mpStateInterface, 0);
    mTimeAnimatorStuntRun.Construct("CurrentTimeAnimStuntRun_cpt", mpStateInterface, 0);
    mScoreAnimator.Construct("Score_anim", mpStateInterface, 0);
    mBankingAnimator.Construct("Banking_anim", mpStateInterface, 0);
    mStuntAnimator.Construct("StuntBar_anim", mpStateInterface, 0);
    mMultiplierAnimator.Construct("Multiplier_anim", mpStateInterface, 0);

    mDistanceInterpolator.Construct("distanceInter_cpt", mpStateInterface, 0);

    ClearEventSpecificData();

    mpEventInfoComponentName   = 0;      // stw 0, 0x14
    mHUDFileRef.mpFileInstance = 0;      // stw 0, 0x18

    mfDistanceWarningThreshold = mpStateInterface->IsUsingMetricUnits()
                               ? KF_DISTANCE_WARNING_THRESHOLD_METRIC
                               : KF_DISTANCE_WARNING_THRESHOLD_IMPERIAL;
}

// ---------------------------------------------------------------------------
// @0x82412C00 (decl BrnEventInfo.h:85) -- bind the component's root clip out of a
// loaded Flapt file and remember the name + file handle the per-animator Prepare
// calls need later.
//
// This writes the BASE mAptRef (+0x04), NOT mEventMovieClip (+0x0C):
// PrepareComponentsForGameMode / MoveAnimation resolve the "Event_mc" child off it.
//
// Call site: RaceMainHudState::OnEnter @0x82478EF8, immediately after Construct.
// ---------------------------------------------------------------------------
void EventInfoComponent::Prepare(const char* lacFullName, const BrnFlapt::FileRef& lFile)
{
    CGS_ASSERT(lacFullName != 0, "lacName != NULL");   // BrnGuiFlaptComponent.h:133 (non-gating)

    lFile.FindComponent(&mAptRef, lacFullName);

    CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");   // BrnFlaptMovieClipRef.h:272 (non-gating)

    mAptRef.mpMovieClipInst->ResetTimeline();

    mpEventInfoComponentName = lacFullName;
    mHUDFileRef              = lFile;
}

// ---------------------------------------------------------------------------
// @0x82412CA8 (decl BrnEventInfo.h:276) -- reset every per-event scalar. Leaf: the
// X360 body has ZERO callees, which is what makes its store list the layout oracle
// for the whole +0x3D0..+0x494 run (see BrnEventInfo.h's _AssertLayout).
//
// Called by Construct @0x82421160, by SetEventType @0x8242FC78 and by
// UpdateFreeBurnLobby @0x8242FE98.
//
// The console interleaves these stores freely (they are independent scalar writes to
// distinct members and the scheduler reorders them); they are written here in member
// order. Every value below is read off the asm, not inferred: the three float
// constants are flt_82001C98 == 1.0f, flt_820037C8 == -1.0f and flt_82001CC0 == 0.0f.
//
// Two members inside the run are deliberately NOT reset, matching the console:
//   maDisplayedStunt[0].miStuntScore -- the store at +0x448 is a 4-byte `stw` that
//     covers only meStuntType (@0x82412D80), never the score half;
//   maiFreeburnChallengeData[2]      -- no store at +0x488/+0x48C anywhere in the body
//     (only the count at +0x490 is cleared).
// mbRoadRageComponentsPrepared (+0x494) is likewise untouched here -- only
// PrepareComponentsForGameMode's road-rage arm writes it.
// ---------------------------------------------------------------------------
void EventInfoComponent::ClearEventSpecificData()
{
    meCurrentTargetMedal = static_cast<BrnGameState::ECurrentMedalTargetTime>(
                               KI_CURRENT_MEDAL_TARGET_TIME_NONE);            // stw 3,  0x3DC
    mfCurrentTimeSinceLastPositionChange = 1.0f;                              // stfs    0x3E0
    mbTimeRemainingFlashing              = false;                             // stb 0,  0x3E4
    miCurrentPosition                    = 0;                                 // stw 0,  0x3E8
    miTotalRacers                        = 0;                                 // stw 0,  0x3EC
    mCurrentLandmark = BrnGameState::LandmarkIndex(KI_INVALID_LANDMARK);      // sth     0x3F0
    mfDistToCheckpoint                   = -1.0f;                             // stfs    0x3F4
    mbNearFinish                         = false;                             // stb 0,  0x3F8
    mfTimeLeft                           = -1.0f;                             // stfs    0x3FC
    miCurrentTakedowns                   = -1;                                // stw -1, 0x400
    miTargetTakedowns                    = -1;                                // stw -1, 0x404
    mRivalCarID                          = 0;                                 // std r11(0), 0x408 (8 bytes)
    miTargetDamageCount                  = -1;                                // stw -1, 0x410
    miCurrentDamageCount                 = -1;                                // stw -1, 0x414
    mfRivalDistanceFromTarget            = -1.0f;                             // stfs    0x418
    miShowTimeCarsCrashed                = -1;                                // stw -1, 0x41C
    mfShowTimeDistanceTravelled          = -1.0f;                             // stfs    0x420
    mfTargetTimeInEvent                  = -1.0f;                             // stfs    0x424
    mfCurrentTimeInEvent                 = -1.0f;                             // stfs    0x428
    miTargetScoreInEvent                 = -1;                                // stw -1, 0x42C
    miCurrentScoreInEvent                = -1;                                // stw -1, 0x430
    mbInShortcut                         = false;                             // stb 0,  0x434
    miCurrentComboInEvent                = -1;                                // stw -1, 0x438
    miMultiplierInEvent                  = 1;                                 // stw 1,  0x43C
    miAddScoreDisplayed                  = -1;                                // stw -1, 0x440
    mbComboWarningActive                 = false;                             // stb 0,  0x444
    maDisplayedStunt[0].meStuntType      = BrnGameState::E_STUNT_TYPE_INVALID; // stw -1, 0x448
    mfTotupStartScore                    = 0.0f;                              // stfs    0x450
    mfTotupFinishScore                   = 0.0f;                              // stfs    0x454
    mfTotupStartMultiplier               = 0.0f;                              // stfs    0x458
    mfTotupFinishMultiplier              = 0.0f;                              // stfs    0x45C
    mfTotupStartTime                     = 0.0f;                              // stfs    0x460
    mbTottingUp                          = false;                             // stb 0,  0x464
    miCurrentEventStateIndex             = 0;                                 // stw 0,  0x468
    miField_46C                          = 0;                                 // stw 0,  0x46C
    mbFreeBurnBasicInfoShowing           = false;                             // stb 0,  0x470
    mbFreeBurnInfoShowing                = false;                             // stb 0,  0x471
    meNextGameMode = BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY; // stw 15, 0x474
    meSecurity                           = KI_BRN_GAME_SECURITY_INVALID;      // stw 3,  0x478
    miNumRounds                          = 0;                                 // stw 0,  0x47C
    mFreeburnChallengeID                 = 0;                                 // std r11(0), 0x480 (8 bytes)
    miFreeburnChallengeDataCount         = 0;                                 // stw 0,  0x490
}

// ---------------------------------------------------------------------------
// @0x8241EDF8 (decl BrnEventInfo.h:373; asserts at BrnEventInfo.h:485) -- drive the
// position/medal text-state animator (mTextStateAnimatorRace, X360 this+0x0F8) to the
// medal frame implied by the finishing position: 1 -> "Gold", 2..3 -> "Normal",
// 4..8 -> "Warning".
// ---------------------------------------------------------------------------
void EventInfoComponent::SetPositionTextState(s32 liPosition)
{
    CGS_ASSERT(liPosition > 0 && liPosition <= 8,
               "Invalid player position provided - ");

    if (liPosition == 1)
    {
        mTextStateAnimatorRace.Run("Gold");
    }
    else if (liPosition < KI_POSITION_WARNING_THRESHOLD)
    {
        mTextStateAnimatorRace.Run("Normal");
    }
    else
    {
        mTextStateAnimatorRace.Run("Warning");
    }
}

// ---------------------------------------------------------------------------
// @0x824218A8 (asserts at BrnEventInfo.cpp:2227) -- pick single- vs multi-digit layout
// for the road-rage takedown counter and drive the digits animator to it. 10+ takedowns
// need two digits.
//
// MEMBER NAME CORRECTED 2026-08-27 (wave A6): the animator this body runs is X360
// this+0x168, which the layout re-grow identifies as the 13th, DWARF-less
// mTakedownNumbersAnimator ("TakedownNumbersAnimator"), NOT mDistanceAnimatorRace --
// that one ("raceDistance_anim") is the NEXT animator, at this+0x1A0. The behaviour is
// unchanged; only the name was wrong.
//
// NOTE: absent from the (PS3-derived) DWARF method list, which instead carries the
// distinct SetTakedownsTextState StrStream method; this body is the X360-attested function.
// ---------------------------------------------------------------------------
void EventInfoComponent::SetTakedownsDigitsState()
{
    CGS_ASSERT(miCurrentTakedowns >= 0,
               "Invalid takedown count provided - ");

    if (miCurrentTakedowns >= 10)
    {
        mTakedownNumbersAnimator.Run("MultipleDigits");
    }
    else
    {
        mTakedownNumbersAnimator.Run("SingleDigit");
    }
}

}

// ============================================================================
// FOLDED FROM BrnEventInfo_wS1.cpp (wave S1) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo_wS1.cpp
//
// BrnGui::EventInfoComponent -- the mode-frame / component-binding half of the
// component, reconstructed from BURNOUT_X360_ARTIST.XEX. Partfile sibling of
// BrnEventInfo.cpp (which owns Construct / Prepare / ClearEventSpecificData /
// SetPositionTextState / SetTakedownsDigitsState); the two share one class and one
// header and must be mounted together.
//
// Bodied here (stunt-run wave S1, 2026-08-27):
//   SetEventType                       @0x8242FC78   (DWARF BrnEventInfo.h:95)
//   MoveAnimation                      @0x82429570   (DWARF BrnEventInfo.h:100)
//   PrepareComponentsForGameMode       @0x82429150   (DWARF BrnEventInfo.h:89)
//   SetTextFieldDangerColour           @0x82421980   (DWARF BrnEventInfo.h:396)
//   BankingTransitionCompleteCallback  @0x824214C8   (DWARF BrnEventInfo.h:389)
//
// This file also OWNS the two .rdata string tables its bodies consume --
// KAC_MODE_FRAME_NAMES (MoveAnimation) and KAPC_TEXTFIELD_NAMES
// (PrepareComponentsForGameMode). The header's other three tables
// (KAPC_EVENT_STATE_NAMES / KAC_STUNT_TYPE_STRING_IDS /
// KAPC_TIMEOUT_WEDGE_FRAME_NAMES) belong to UpdateStuntAttack @0x82429C08 and are
// deliberately NOT defined here -- exactly one TU may define each.
//
// THE ONE THING THIS FILE UN-LATCHES. BankingTransitionCompleteCallback is the only
// writer that clears mbTottingUp. UpdateStuntAttack sets it on the banking edge and
// never clears it; without this callback registered (which
// PrepareComponentsForGameMode's stunt arm does) the readout freezes into the
// totting-up lerp forever after the first bank.
// ============================================================================

namespace BrnGui
{

// ---------------------------------------------------------------------------
// The .rdata tables consumed by this TU. Contents read out of the X360 image
// (image.bin, big-endian; file offset = VA - 0x82000000) on 2026-08-27 -- these are
// the real console strings, not reconstructions.
// ---------------------------------------------------------------------------

// off_82F24AB0 [18] -- the outer-clip frame label per EGameModeType, indexed directly
// by meCurrentEventType in MoveAnimation. Repeats are console-faithful (mode 1 FaceOff
// and mode 10 OnlineRace both ride the "Race" frame, the four traffic-ish modes share
// "TrafficAttack", etc). Slot 15 (E_MODE_ONLINE_FREE_BURN_LOBBY) is "Invisible" and is
// NEVER read: MoveAnimation short-circuits mode 15 onto its own two labels.
const char* const EventInfoComponent::KAC_MODE_FRAME_NAMES[EventInfoComponent::KI_MODE_FRAME_NAME_COUNT] =
{
    "Race",             //  0  E_MODE_OFFLINE_RACE
    "Race",             //  1  E_MODE_FACE_OFF
    "ShowTime",         //  2  E_MODE_OFFLINE_SHOWTIME
    "RoadRage",         //  3  E_MODE_ROAD_RAGE
    "Pursuit",          //  4  E_MODE_PURSUIT
    "BurningRoute",     //  5  E_MODE_BURNING_ROUTE
    "Eliminator",       //  6  E_MODE_ELIMINATOR
    "TrafficAttack",    //  7  E_MODE_STUNT_ATTACK
    "Survivor",         //  8  E_MODE_MARKED_MAN
    "TrafficAttack",    //  9  E_MODE_TRAFFIC_ATTACK
    "Race",             // 10  E_MODE_ONLINE_RACE
    "RoadRage",         // 11  E_MODE_ONLINE_ROAD_RAGE
    "TrafficAttack",    // 12  E_MODE_ONLINE_FUGITIVE
    "RoadRage",         // 13  E_MODE_ONLINE_BURNING_HOME_RUN
    "TrafficAttack",    // 14  E_MODE_ONLINE_FREE_BURN
    "Invisible",        // 15  E_MODE_ONLINE_FREE_BURN_LOBBY (unreachable -- see above)
    "ShowTime",         // 16  E_MODE_ONLINE_SHOWTIME
    "TrafficAttack"     // 17  E_MODE_ONLINE_MODE_END
};

// off_82F24B4C [7] -- the generic text-field slots PrepareComponentsForGameMode binds
// out of mEventMovieClip on EVERY mode. The 7th entry is the X360-only slot the
// DWARF/PS3 build lacks (UpdateStuntAttack's online add-score delta lives in it).
const char* const EventInfoComponent::KAPC_TEXTFIELD_NAMES[EventInfoComponent::KI_TEXTFIELD_COUNT] =
{
    "textField_1_mc",
    "textField_2_mc",
    "textField_3_mc",
    "textField_4_mc",
    "textField_5_mc",
    "textField_6_mc",
    "textField_7_mc"
};

namespace
{
    // The apt child-component / text-box names the assert strings at
    // BrnEventInfo.cpp:380 and :383 spell out by name.
    const char* const KAC_TEXTBOX_NAME             = "TextField_txt";
    const char* const KAC_ADDSCORE_TEXTFIELD_NAME  = "textField_addScore_mc";
    const char* const KAC_BANKSCORE_TEXTFIELD_NAME = "textField_bankScore_mc";
    const char* const KAC_BANKSCORE_TEXTBOX_NAME   = "BankTextField_txt";

    // unk_820046A7 -- the image holds a bare NUL there, i.e. the empty string. Every
    // "blank this field" store in PrepareComponentsForGameMode passes it.
    const char* const KAC_EMPTY_STRING = "";

    // The child clip MoveAnimation re-binds out of the mode frame every time.
    const char* const KAC_EVENT_MOVIECLIP_NAME = "Event_mc";

    // The two mode-15 frame labels MoveAnimation picks between instead of
    // KAC_MODE_FRAME_NAMES[15].
    const char* const KAC_FREEBURN_LOBBY_FRAME_NAME       = "FreeBurnLobby";
    const char* const KAC_FREEBURN_LOBBY_EVENT_FRAME_NAME = "FreeBurnLobbyEvent";

    // The event-state label SetEventType moves to. It is KAPC_EVENT_STATE_NAMES[0]
    // (off_82F24B68[0]); the X360 loads the bare string pointer here rather than
    // indexing the table, so the literal is reproduced rather than the table read --
    // which also keeps that table's single definition with UpdateStuntAttack.
    const char* const KAC_PREWAIT_STATE_NAME = "prewait";

    // The interpolator child text field the race arm tweens (aRacedistanceTx).
    const char* const KAC_RACE_DISTANCE_TEXTFIELD_NAME = "raceDistance_txt";

    // SetTextFieldDangerColour's ramp window. The X360 folds the reciprocal into
    // flt_82004014 (0x3DCCCCCD == 0.1f exactly), so the divide never appears in the
    // asm; both halves are spelled here so the 10-second window is legible. The same
    // 10.0f gates the timer-flash switch in UpdateStuntAttack.
    const f32 KF_DANGERTIME_START             = 10.0f;
    const f32 KF_ONE_OVER_DANGERTIME_START    = 1.0f / KF_DANGERTIME_START;   // flt_82004014 == 0x3DCCCCCD

    const f32 KF_DANGER_PROGRESSION_MIN       = 0.0f;      // flt_82001CC0
    const f32 KF_DANGER_PROGRESSION_MAX       = 1.0f;      // flt_82001C98

    // ------------------------------------------------------------------------
    // [FLAG placeholder constants -- SIX dynamically-initialised Vector4/Vector2s]
    //
    // KV4_SAFECOLOUR @0x82FB29E0 and KV4_DANGERCOLOUR @0x82FB2AC0 (read by
    // SetTextFieldDangerColour), and the four SetInterpValues endpoints
    // @0x82FB2A00 / @0x82FB2A90 / @0x82FB2BE0 / @0x82FB2D70 (read by
    // PrepareComponentsForGameMode's race arm), CANNOT be read out of the image:
    // 0x82FB0000..0x82FB8000 is 32 KiB of zeroes in image.bin and every reference to
    // that range across the whole .ida-exports set is a READ -- these are file-scope
    // objects with Vector4 constructors, filled by a static initialiser that IDA never
    // exported. A zero read there is NOT the console value (see the 2026-08-23 tooling
    // note: ".bss zero does not mean the console value is zero").
    //
    // The values below are therefore STAND-INS, chosen to be honest about being wrong:
    //   * the colour pair is white -> red, the conventional apt "safe/danger" text ramp;
    //   * the four interpolator endpoints are IDENTITY at both ends, which makes the
    //     race-distance tween a deliberate no-op rather than a plausible-looking lie.
    // Nothing on the stunt-run path (mode 7) reads the interpolator four.
    //
    // DELETE-WHEN the static-initialiser that writes 0x82FB29E0 / 0x82FB2AC0 /
    // 0x82FB2A00 / 0x82FB2A90 / 0x82FB2BE0 / 0x82FB2D70 is located (a `stvx` sweep of
    // the unexported init range, or a live-memory dump of those six VAs on console)
    // and the real lanes are pasted in.
    // ------------------------------------------------------------------------
    const Vector4 KV4_SAFECOLOUR   = { 1.0f, 1.0f, 1.0f, 1.0f };   // [FLAG] unk_82FB29E0
    const Vector4 KV4_DANGERCOLOUR = { 1.0f, 0.0f, 0.0f, 1.0f };   // [FLAG] unk_82FB2AC0

    const Vector4 KV4_RACE_DISTANCE_START_COLOUR = { 1.0f, 1.0f, 1.0f, 1.0f };  // [FLAG] unk_82FB2A00 (vector arg v1)
    const Vector4 KV4_RACE_DISTANCE_END_COLOUR   = { 1.0f, 1.0f, 1.0f, 1.0f };  // [FLAG] unk_82FB2A90 (vector arg v2)
    const Vector2 KV2_RACE_DISTANCE_START_SCALE  = { 1.0f, 1.0f, 0.0f, 0.0f };  // [FLAG] unk_82FB2BE0 (vector arg v3)
    const Vector2 KV2_RACE_DISTANCE_END_SCALE    = { 1.0f, 1.0f, 0.0f, 0.0f };  // [FLAG] unk_82FB2D70 (vector arg v4)
}

// ---------------------------------------------------------------------------
// @0x8242FC78 (decl BrnEventInfo.h:95; assert at BrnEventInfo.cpp:501) -- latch the
// event's game mode, move the panel to its "prewait" state and wipe the per-event
// scalars.
//
// Three statements, in this order: the mode is stored BEFORE MoveAnimation runs
// (MoveAnimation reads it to pick the frame), and ClearEventSpecificData runs AFTER
// (so the freshly bound components start from the cleared scalars).
//
// Sole call site: RaceMainHudState::SetupEventInfo @0x82474A60, which then follows up
// with MoveAnimation("transin") for every mode except E_MODE_ONLINE_FREE_BURN_LOBBY.
// ---------------------------------------------------------------------------
void EventInfoComponent::SetEventType(BrnGameState::GameStateModuleIO::EGameModeType leCurrentGameMode)
{
    CGS_ASSERT(leCurrentGameMode != BrnGameState::GameStateModuleIO::E_MODE_NONE,
               "GsmIO::E_MODE_NONE != leCurrentGameMode");   // cpp:501 (non-gating)

    meCurrentEventType = leCurrentGameMode;                  // stw r30, 0x3D4(r31)

    MoveAnimation(KAC_PREWAIT_STATE_NAME);
    ClearEventSpecificData();
}

// ---------------------------------------------------------------------------
// @0x82429570 (decl BrnEventInfo.h:100; asserts at BrnEventInfo.cpp:522/:523 and
// BrnFlaptMovieClipRef.h:272) -- drive the panel to a mode frame and then to an
// animation state inside it.
//
// The console does FIVE things here, and the middle three are why this is not a
// one-line goto:
//   1. goto-and-play the MODE frame on the OUTER clip (the base mAptRef);
//   2. re-resolve the "Event_mc" child ON THAT FRAME and re-seat mEventMovieClip --
//      the child object is frame-specific, so the handle held from the previous mode
//      is stale the instant step 1 lands;
//   3. reset the child's timeline;
//   4. re-run PrepareComponentsForGameMode, which re-binds every text field and
//      animator against the new child;
//   5. goto-and-play the requested STATE label on the child.
// ⭐ Consequence for callers: every MoveAnimation invalidates every previously cached
// TextFieldInstance / animator clip pointer in this component. Nothing may cache
// across a call.
//
// The mode-15 special case reads the 8-byte mFreeburnChallengeID (X360 `ld r11,
// 0x480(r30)` + `cmpldi`), NOT a 4-byte flag: IDA's pseudocode prints it as
// `*(a1 + 1156)` (== +0x484, the SECOND word), which on a little-endian host would
// read the wrong half of the handle. The asm is authoritative -- this is the whole
// 64-bit id, tested against zero.
// ---------------------------------------------------------------------------
void EventInfoComponent::MoveAnimation(const char* lpcAnimation)
{
    CGS_ASSERT(lpcAnimation != 0, "lpcAnimation");   // cpp:522 (non-gating)

    // cmpwi -1 / ble -> fire, cmpwi 0x12 / blt -> pass. The upper bound is the X360's
    // EIGHTEEN modes (KI_MODE_FRAME_NAME_COUNT), which is the bound the mode-frame
    // table is actually sized to -- the shared enum's E_MODE_COUNT spells 17.
    CGS_ASSERT((meCurrentEventType > BrnGameState::GameStateModuleIO::E_MODE_NONE)
            && (meCurrentEventType < KI_MODE_FRAME_NAME_COUNT),
               "( GsmIO::E_MODE_NONE < meCurrentEventType ) && ( GsmIO::E_MODE_COUNT > meCurrentEventType )");   // cpp:523 (non-gating)

    const char* lpcFrame;

    if (meCurrentEventType == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
    {
        // A challenge in flight, or the per-event panel not showing, keeps the plain
        // lobby frame; only "no challenge AND the event panel showing" reaches the
        // event frame.
        lpcFrame = ((mFreeburnChallengeID != 0) || !mbFreeBurnInfoShowing)
                 ? KAC_FREEBURN_LOBBY_FRAME_NAME
                 : KAC_FREEBURN_LOBBY_EVENT_FRAME_NAME;
    }
    else
    {
        lpcFrame = KAC_MODE_FRAME_NAMES[meCurrentEventType];
    }

    mAptRef.GotoAndPlayLabel(lpcFrame);              // sub_8246F3E8 on this+4

    // The X360 hands FindChildMovieClipOnFrame a stack MovieClipRef and then copies
    // BOTH words of the returned ref into mEventMovieClip (+0x0C/+0x10). Preserved.
    BrnFlapt::MovieClipRef lEventMovieClip;
    mEventMovieClip = *mAptRef.FindChildMovieClipOnFrame(&lEventMovieClip, KAC_EVENT_MOVIECLIP_NAME);

    CGS_ASSERT(mEventMovieClip.mpMovieClipInst != 0, "mpMovieClipInst");   // BrnFlaptMovieClipRef.h:272 (non-gating)

    mEventMovieClip.mpMovieClipInst->ResetTimeline();

    PrepareComponentsForGameMode();

    mEventMovieClip.GotoAndPlayLabel(lpcAnimation);  // sub_8246F3E8 on this+0xC
}

// ---------------------------------------------------------------------------
// @0x82429150 (decl BrnEventInfo.h:89; asserts at BrnEventInfo.cpp:380/:383) -- bind
// every apt handle this component will touch for the CURRENT mode.
//
// Two halves. First the seven generic KAPC_TEXTFIELD_NAMES slots, bound out of
// mEventMovieClip and blanked, unconditionally for every mode. Then an 18-case switch
// with arms for only FOUR mode groups -- 0 (race), 3 (road rage), 5 (burning route)
// and {7, 12, 14, 17} (stunt attack / stunt run, offline and online). Every other mode
// gets the seven generic fields and nothing else; that is the console's default arm,
// not a gap in the reconstruction (the jump table at 0x824291E4 routes cases
// 1, 2, 4, 6, 8-11, 13, 15 and 16 straight to the epilogue).
//
// Called only by MoveAnimation @0x82429570, i.e. on every mode/state move.
// ---------------------------------------------------------------------------
void EventInfoComponent::PrepareComponentsForGameMode()
{
    // The X360 walks off_82F24B4C..off_82F24B68 with a name cursor and a 0x0C-stride
    // TextFieldRef cursor rooted at this+0x1C; on the host the same seven slots are
    // addressed BY INDEX (TextFieldRef widens on x64, so no stride may be assumed).
    for (s32 liTextField = 0; liTextField < KI_TEXTFIELD_COUNT; ++liTextField)
    {
        TryFindTextFieldFromMovieClip(mEventMovieClip,
                                      KAPC_TEXTFIELD_NAMES[liTextField],
                                      KAC_TEXTBOX_NAME,
                                      &maTextField[liTextField]);

        // Note the console re-reads the OUT ref's instance word (`lwz r11, 0(r30)`)
        // rather than testing the returned bool. Same answer, but transcribed as
        // written -- a missing field is silently skipped, never asserted, here.
        if (maTextField[liTextField].mpTextFieldInstance != 0)
        {
            maTextField[liTextField].SetText(KAC_EMPTY_STRING, false);
        }
    }

    switch (meCurrentEventType)
    {
    // ---- race ----------------------------------------------------------
    case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:
        mDistanceInterpolator.Prepare("distanceInter_cpt", mHUDFileRef, mpEventInfoComponentName);

        // Four vector arguments in v1..v4 -- start colour, end colour, start scale,
        // end scale. See the [FLAG placeholder constants] block: all four are
        // dynamically-initialised objects the image cannot supply.
        mDistanceInterpolator.SetInterpValues(KAC_RACE_DISTANCE_TEXTFIELD_NAME,
                                              KV4_RACE_DISTANCE_START_COLOUR,
                                              KV4_RACE_DISTANCE_END_COLOUR,
                                              KV2_RACE_DISTANCE_START_SCALE,
                                              KV2_RACE_DISTANCE_END_SCALE);

        mTextStateAnimatorRace.Prepare("raceTextState_anim", mHUDFileRef, mpEventInfoComponentName);
        mDistanceAnimatorRace.Prepare("raceDistance_anim", mHUDFileRef, mpEventInfoComponentName);
        break;

    // ---- road rage -----------------------------------------------------
    case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:
        mTextStateAnimatorRRage.Prepare("rRageTextState_anim", mHUDFileRef, mpEventInfoComponentName);

        // `stb r9(1), 0x494(r31)` @0x82429328 -- issued between the two Prepare calls,
        // before the takedown-numbers animator's virtual dispatch. Kept in place.
        mbRoadRageComponentsPrepared = true;

        mTakedownNumbersAnimator.Prepare("TakedownNumbersAnimator", mHUDFileRef, mpEventInfoComponentName);
        mTimeAnimatorRRage.Prepare("CurrentTimeAnimRRage_cpt", mHUDFileRef, mpEventInfoComponentName);

        // `addi r3, r31, 0x34` -> (0x34 - 0x1C) / 0x0C == slot 2.
        maTextField[2].SetAutoSize(true);
        break;

    // ---- burning route -------------------------------------------------
    case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:
        mTimeAnimatorBRoute.Prepare("CurrentTimeAnimBRoute_cpt", mHUDFileRef, mpEventInfoComponentName);
        break;

    // ---- stunt attack / stunt run (offline + the three online flavours) --
    case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:        // 7
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:     // 12
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:    // 14
    case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:     // 17
    {
        // Both of these DO assert on failure (unlike the seven generic slots above),
        // and both blank the field afterwards whether or not the bind succeeded.
        const bool lbFoundAddScore =
            TryFindTextFieldFromMovieClip(mEventMovieClip,
                                          KAC_ADDSCORE_TEXTFIELD_NAME,
                                          KAC_TEXTBOX_NAME,
                                          &mAddScoreTextField);
        CGS_ASSERT(lbFoundAddScore,
                   "TryFindTextFieldFromMovieClip( mEventMovieClip, KAC_ADDSCORE_TEXTFIELD_NAME, KAC_TEXTBOX_NAME, &mAddScoreTextField )");   // cpp:380 (non-gating)

        mAddScoreTextField.SetText(KAC_EMPTY_STRING, false);

        // The bank field lives in its own text box ("BankTextField_txt"), not the
        // generic "TextField_txt" -- the one asymmetry in this arm.
        const bool lbFoundBankScore =
            TryFindTextFieldFromMovieClip(mEventMovieClip,
                                          KAC_BANKSCORE_TEXTFIELD_NAME,
                                          KAC_BANKSCORE_TEXTBOX_NAME,
                                          &mBankScoreTextField);
        CGS_ASSERT(lbFoundBankScore,
                   "TryFindTextFieldFromMovieClip( mEventMovieClip, KAC_BANKSCORE_TEXTFIELD_NAME, KAC_BANKSCORE_TEXTBOX_NAME, &mBankScoreTextField )");   // cpp:383 (non-gating)

        mBankScoreTextField.SetText(KAC_EMPTY_STRING, false);

        mAddScoreAnimator.Prepare("AddScore_anim", mHUDFileRef, mpEventInfoComponentName);
        mScoreBackgroundAnimator.Prepare("ScoreBacking_anim", mHUDFileRef, mpEventInfoComponentName);
        mScoreAnimator.Prepare("Score_anim", mHUDFileRef, mpEventInfoComponentName);
        mBankingAnimator.Prepare("Banking_anim", mHUDFileRef, mpEventInfoComponentName);

        // THE un-latch wire. Registered on the OUTER clip (this+4), not on
        // mEventMovieClip: the banking transition's end-frame trigger lives on the
        // mode frame itself. lpUserData is this component.
        mAptRef.SetFrameTriggerCallback(
            reinterpret_cast<void*>(&EventInfoComponent::BankingTransitionCompleteCallback),
            this);

        mStuntAnimator.Prepare("StuntBar_anim", mHUDFileRef, mpEventInfoComponentName);
        mMultiplierAnimator.Prepare("Multiplier_anim", mHUDFileRef, mpEventInfoComponentName);
        mTimeAnimatorStuntRun.Prepare("CurrentTimeAnimStuntRun_cpt", mHUDFileRef, mpEventInfoComponentName);

        // `stw r11(0), 0x468(r31)` -- drop the cached event-state index so
        // UpdateStuntAttack's first tick after a re-bind always re-issues its
        // MoveAnimation (the animator it would otherwise skip is a fresh object).
        miCurrentEventStateIndex = 0;
        break;
    }

    default:
        // Cases 1, 2, 4, 6, 8-11, 13, 15, 16 -- the jump table's default arm. The seven
        // generic text fields above are all these modes get.
        break;
    }
}

// ---------------------------------------------------------------------------
// @0x82421980 (decl BrnEventInfo.h:396) -- ramp a text field's colour from
// KV4_SAFECOLOUR to KV4_DANGERCOLOUR over the last KF_DANGERTIME_START (10) seconds of
// an event, and apply it.
//
// progression = Clamp( 1 - lfTimeLeft / 10, 0, 1 ), so it is 0 while more than ten
// seconds remain and reaches 1 exactly at zero. The console spells the clamp as two
// `fsel`s, transcribed below as the branches they encode:
//   fneg f11,f13 ; fsel f13,f11,f12,f13   ->  (-x >= 0) ? 0 : x   ==  x <= 0 -> 0
//   fsubs f12,f0,f13 ; fsel f0,f12,f13,f0 ->  (1-x >= 0) ? x : 1  ==  x >  1 -> 1
//
// Callers: UpdateRoadRage @0x82429A48, UpdateStuntAttack @0x82429C08 (every timer
// tick) and UpdateBurningRoute @0x8242A830.
//
// NOTE the PPC signature: this+r3, lpTextField+r4, lfTimeLeft in f1 with the GPR slot
// SKIPPED -- IDA prints it as `(double a1, int a2, int a3)` with the float FIRST. The
// declared C++ order is (lpTextField, lfTimeLeft), per the DWARF.
// ---------------------------------------------------------------------------
void EventInfoComponent::SetTextFieldDangerColour(TextFieldComponentType* lpTextField, f32 lfTimeLeft)
{
    // fnmsubs f13, f1, f13, f0 == -( lfTimeLeft * 0.1f - 1.0f ).
    f32 lfProgression = KF_DANGER_PROGRESSION_MAX - (lfTimeLeft * KF_ONE_OVER_DANGERTIME_START);

    if (lfProgression <= KF_DANGER_PROGRESSION_MIN)
    {
        lfProgression = KF_DANGER_PROGRESSION_MIN;
    }

    if (lfProgression > KF_DANGER_PROGRESSION_MAX)
    {
        lfProgression = KF_DANGER_PROGRESSION_MAX;
    }

    // vsubfp v13, DANGER, SAFE ; vspltw v12, progression ; vmaddfp v1, v13, v12, v0
    // == SAFE + (DANGER - SAFE) * progression, per RGBA lane. Written lane-by-lane in
    // the house style (this tree's Vector4 is the plain four-float rw::math::vpu POD,
    // and FlaptInterpolatorComponent::SetProportion lerps the same way).
    Vector4 lv4NewColour;
    lv4NewColour.x = KV4_SAFECOLOUR.x + (KV4_DANGERCOLOUR.x - KV4_SAFECOLOUR.x) * lfProgression;
    lv4NewColour.y = KV4_SAFECOLOUR.y + (KV4_DANGERCOLOUR.y - KV4_SAFECOLOUR.y) * lfProgression;
    lv4NewColour.z = KV4_SAFECOLOUR.z + (KV4_DANGERCOLOUR.z - KV4_SAFECOLOUR.z) * lfProgression;
    lv4NewColour.w = KV4_SAFECOLOUR.w + (KV4_DANGERCOLOUR.w - KV4_SAFECOLOUR.w) * lfProgression;

    lpTextField->SetColour(lv4NewColour);
}

// ---------------------------------------------------------------------------
// @0x824214C8 (decl BrnEventInfo.h:389; assert at BrnEventInfo.cpp:736) -- the apt
// frame trigger that ends a banking transition.
//
// ⭐ THE UN-LATCH. mbTottingUp is set true by UpdateStuntAttack on the banking edge
// (combo warning falling + the combo count dropping) and is cleared NOWHERE ELSE in
// the whole 26-function set. With this callback unregistered or unbodied, the first
// bank of a stunt run pins the readout into the totting-up lerp permanently: the score
// and multiplier fields stay slaved to mfTotupStart*/mfTotupFinish* and the live combo
// path never runs again. The registration half lives in
// PrepareComponentsForGameMode's stunt arm above; both halves are required.
//
// SHAPE: the X360 call is SINGLE-argument (`mr r31, r3` and nothing reads r4) -- this
// is a plain function pointer handed to MovieClipRef::SetFrameTriggerCallback, so it is
// static and recovers the component from lpUserData. The MovieClipInstance callback
// type is void(*)(void*, u16); luArg is accepted and ignored, exactly as the console
// ignores r4.
//
// The assert is non-gating on console -- the Run below dereferences lpUserData whether
// or not it fired. Transcribed as written.
// ---------------------------------------------------------------------------
void EventInfoComponent::BankingTransitionCompleteCallback(void* lpUserData, u16 luArg)
{
    (void)luArg;

    CGS_ASSERT(lpUserData != 0, "lpUserData");   // cpp:736 (non-gating)

    EventInfoComponent* const lpThis = static_cast<EventInfoComponent*>(lpUserData);

    lpThis->mBankingAnimator.Run("notBanking");   // this+0x2B8
    lpThis->mbTottingUp = false;                  // stb 0, 0x464
}

}

// ============================================================================
// FOLDED FROM BrnEventInfo_wS2.cpp (wave S2) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo_wS2.cpp
//
// BrnGui::EventInfoComponent -- THE STUNT READOUT. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (wave A8, 2026-08-27). Bodied here:
//
//   Update             @0x82435430   the per-frame mode switch (jpt_824354A8, 18 cases)
//   UpdateStuntAttack  @0x82429C08   the score / multiplier / timer / combo readout
//   UpdateCrash        @0x82412E98   the SHOWTIME readout (added 2026-08-29 with the
//                                    showtime score chain: distance travelled + cars crashed)
//
// plus the three .rdata string tables ONLY these two bodies consume
// (KAPC_EVENT_STATE_NAMES, KAC_STUNT_TYPE_STRING_IDS, KAPC_TIMEOUT_WEDGE_FRAME_NAMES),
// read verbatim out of the X360 image. The other two tables the header declares
// (KAC_MODE_FRAME_NAMES @0x82F24AB0, KAPC_TEXTFIELD_NAMES @0x82F24B4C) belong with
// MoveAnimation / PrepareComponentsForGameMode and are deliberately NOT defined here.
//
// Partfile of BrnEventInfo.cpp (Construct / Prepare / ClearEventSpecificData /
// SetPositionTextState / SetTakedownsDigitsState live there). Split out because the two
// bodies below are ~600 lines of X360 pseudocode between them.
//
// ---------------------------------------------------------------------------
// THINGS THE IDA PSEUDOCODE GETS WRONG HERE -- all three verified against the asm:
//
//  (1) UpdateStuntAttack takes TWO real arguments. `clrlwi r26, r5, 24` @0x82429C1C
//      is the bool; Update's jump table passes `li r5, 0` for case 7 (offline stunt
//      run) @0x82435534 and `li r5, 1` for cases 12 / 14 / 17 @0x82435548. The DecFIGS
//      DWARF's one-argument form (BrnEventInfo.h:463) is a stale-build artefact.
//
//  (2) The combo-warning wedge index is a TRUNCATED FLOAT, not an int read. The asm is
//      `lfsx f0, r23, 0xAC6C` / `fctiwz f0, f0` / `stfiwx` @0x8242A04C-0x8242A054 --
//      GuiCache::mfComboWarningTimeActive is an f32 and the wedge frame is its integer
//      part. IDA prints it as `v35 = *(a2 + 44140)`, i.e. the bit pattern. Reading it
//      that way is exactly the "X360 value on the x64 host" class of bug this project
//      keeps hitting; the truncation is transcribed.
//
//  (3) The two BOOSTTRICK SetLocalisedText calls declare THREE parameter pairs and IDA
//      prints only two. The third pair lives in the X360 parameter save area:
//      `stw r7, 0x54(r1)` (== the SnPrintf'd score buffer) and `stw r9, 0x5C(r1)`
//      (== 0xB, E_FORMAT_INTEGER) @0x8242A658/@0x8242A670 (and the identical pair at
//      @0x8242A6E8/@0x8242A700). With the save area based at r1+0x10 and 8-byte
//      right-justified slots, +0x50 and +0x58 are parameters 9 and 10 -- so the tail is
//      (lacStuntScore, E_FORMAT_INTEGER), the same value the plain TRICK arm passes.
//
// NO TIME FORMATTER IS INVOLVED. The clock is built with CgsCore::SPrintf("%dm%02ds")
// and pushed through plain TextFieldRef::SetText; the only ParameterFormatTypes anywhere
// in this TU are 9 (E_FORMAT_TEXT_DATABASE_LOOKUP) and 11 (E_FORMAT_INTEGER), both of
// which are bodied. The outstanding CgsLanguage seconds/distance formatter debt does not
// touch this readout.
// ============================================================================

namespace BrnGui
{

// ---------------------------------------------------------------------------
// .rdata tables (X360 image reads, 2026-08-27, big-endian at file offset VA-0x82000000).
// ---------------------------------------------------------------------------

// off_82F24B68 [6] -- the event-state animation names UpdateStuntAttack's mode-frame
// latch drives through MoveAnimation. Index 0 ("prewait") is what SetEventType uses;
// the readout only ever picks 1..5. The DecFIGS DWARF has no symbol for this table.
const char* const EventInfoComponent::KAPC_EVENT_STATE_NAMES[6] =
{
    "prewait",          // 0  (SetEventType @0x8242FC78; never selected by the readout)
    "idle",             // 1  offline stunt run, and the online default
    "YourBlueLeads",    // 2  online, player team == 2, target <= current
    "YourRedLeads",     // 3  online, player team != 2, target <= current
    "RivalBlueLeads",   // 4  online, player team != 2, target >  current
    "RivalRedLeads"     // 5  online, player team == 2, target >  current
};

// off_82F24B80 [18] -- the localisation id per stunt type. EIGHTEEN entries, not the
// DWARF's fifteen: the bound is `cmplwi r30, 0x12` @0x8242A5E8, and the two boost-trick
// prefixes are addressed as &table[8] (off_82F24BA0, "HUD_INFO_STUNT_BOOST") and
// &table[14] (off_82F24BB8, "HUD_INFO_STUNT_REV_TAKEOFF") -- exact index arithmetic that
// pins the stride and the base. Entries 0..14 line up with BrnGameState::EStuntType;
// 15..17 are this table's own extra rows (the scorer's enum spells 15..18 as its
// error/rating sentinels, which is a DIFFERENT list -- see the header note).
const char* const EventInfoComponent::KAC_STUNT_TYPE_STRING_IDS[18] =
{
    "HUD_INFO_STUNT_SPIN",         //  0  E_STUNT_TYPE_SPIN
    "HUD_INFO_STUNT_ROLL",         //  1  E_STUNT_TYPE_BARREL_ROLL
    "HUD_INFO_STUNT_AIR",          //  2  E_STUNT_TYPE_AIR
    "HUD_INFO_STUNT_DRIFT",        //  3  E_STUNT_TYPE_DRIFT
    "HUD_INFO_STUNT_SUPER_JUMP",   //  4  E_STUNT_TYPE_SUPER_JUMP
    "HUD_INFO_STUNT_SMASH",        //  5  E_STUNT_TYPE_SUPER_SMASH
    "HUD_INFO_STUNT_BILLBOARD",    //  6  E_STUNT_TYPE_BILLBOARD
    "HUD_INFO_STUNT_BURNOUT",      //  7  E_STUNT_TYPE_BURNOUT
    "HUD_INFO_STUNT_BOOST",        //  8  E_STUNT_TYPE_BOOST           (off_82F24BA0)
    "HUD_INFO_STUNT_REVERSE",      //  9  E_STUNT_TYPE_REVERSE_DRIVING
    "HUD_INFO_STUNT_HANDBRAKE",    // 10  E_STUNT_TYPE_HANDBRAKE_TURN
    "HUD_INFO_STUNT_POWER_PARK",   // 11  E_STUNT_TYPE_POWER_PARK
    "HUD_INFO_STUNT_CRASH",        // 12  E_STUNT_TYPE_CRASH_FINISH
    "HUD_INFO_STUNT_PROP",         // 13  E_STUNT_TYPE_PROP
    "HUD_INFO_STUNT_REV_TAKEOFF",  // 14  E_STUNT_TYPE_REVERSE_TAKEOFF (off_82F24BB8)
    "HUD_INFO_STUNT_TAKEDOWN",     // 15  (table-only row)
    "HUD_INFO_STUNT_LEAP_CAR",     // 16  (table-only row)
    "HUD_INFO_STUNT_IN_CONVOY"     // 17  (table-only row)
};

// off_82F24BD4 [5] -- the combo-timeout wedge frames the score animator runs as the
// combo window drains. KI_NUM_TIMEOUT_WEDGE_FRAMES == 5 (`cmpwi r29, 5` @0x8242A080).
const char* const EventInfoComponent::KAPC_TIMEOUT_WEDGE_FRAME_NAMES[5] =
{
    "startpulse",
    "startpulse_wedge2",
    "startpulse_wedge3",
    "startpulse_wedge4",
    "startpulse_wedge5"
};

namespace
{
    // CgsLanguage::LanguageManager::ParameterFormatType, as the raw integers the X360
    // passes and this component's SetLocalisedText call sites take (the same convention
    // BrnFlaptTextFieldRef.h documents).
    const s32 KI_FORMAT_TEXT_DATABASE_LOOKUP = 9;    // E_FORMAT_TEXT_DATABASE_LOOKUP (FindString)
    const s32 KI_FORMAT_INTEGER              = 11;   // E_FORMAT_INTEGER (FormatIntegerString @0x828610B0)
    // [showtime score wave 2026-08-29] The third format this TU now uses: UpdateCrash's
    // distance field. `li r5, 0x11` @0x82412ECC == 17 == E_FORMAT_SMALL_DISTANCE, which
    // LanguageManager::FormatText(f32) routes to FormatSmallDistanceString @0x82861988 --
    // metres scaled by mrSmallDistanceConversion, TRUNCATED to an integer, printed into
    // mpDistanceFormatShort. (That leaf was a __debugbreak() trap stub until the pause-stats
    // wave bodied it earlier today; a stub there would have killed the process on the first
    // metre driven in showtime, not merely blanked the field.)
    const s32 KI_FORMAT_SMALL_DISTANCE       = 17;   // E_FORMAT_SMALL_DISTANCE (FormatSmallDistanceString @0x82861988)
    const s32 KI_FORMAT_LARGE_DISTANCE       = 19;   // E_FORMAT_LARGE_DISTANCE

    // The event-state indices the mode-frame latch picks (KAPC_EVENT_STATE_NAMES rows).
    const s32 KI_EVENT_STATE_IDLE              = 1;
    const s32 KI_EVENT_STATE_YOUR_BLUE_LEADS   = 2;
    const s32 KI_EVENT_STATE_YOUR_RED_LEADS    = 3;
    const s32 KI_EVENT_STATE_RIVAL_BLUE_LEADS  = 4;
    const s32 KI_EVENT_STATE_RIVAL_RED_LEADS   = 5;

    // GsmIO::EPlayerTeam value the latch tests for (`cmpwi cr6, r3, 2` @0x82429C4C).
    // The team enum has no home header in the tree yet, so the attested integer stands,
    // exactly as BrnGuiCache::GetCurrentOnlinePlayerTeam's own s32 return does.
    const s32 KI_PLAYER_TEAM_BLUE = 2;

// (fold: an identical definition of KF_DANGERTIME_START was dropped here -- this TU defines it once, above)

    // The clock split. flt_820139F8 is the reciprocal the console multiplies by;
    // flt_82004C6C is the minute it subtracts back out (`fnmsubs` @0x82429F10).
    const f32 KF_ONE_OVER_SECONDS_PER_MINUTE = 0.016666668f;  // 0x3C888889
    const f32 KF_SECONDS_PER_MINUTE          = 60.0f;         // 0x42700000

    // DWARF BrnEventInfo.h:89. The banking tot-up ramp length. The X360 folded the
    // divide away entirely: the clamp ceiling is the bare literal 1.0f (flt_82001C98,
    // @0x8242A2D8) and there is no fdivs and no reciprocal multiply anywhere in the
    // tot-up block -- which is only possible if the constant is exactly 1.0f.
    const f32 KF_TOTUP_DURATION = 1.0f;                       // 0x3F800000

    // IntRound's bias (flt_82001DA0, `fadds` @0x8242A2F4/@0x8242A2F8).
    const f32 KF_INT_ROUND_BIAS = 0.5f;                       // 0x3F000000

    // CgsCore::SPrintf's stack buffer (v96[128], `li r4, 0x80`) and the stunt-score
    // SnPrintf's (v97[64], capped at `li r4, 0x3F` == 63).
    const s32 KI_TEXT_BUFFER_SIZE        = 128;
    const s32 KI_STUNT_SCORE_BUFFER_SIZE = 64;
    const s32 KI_STUNT_SCORE_PRINT_CAP   = 63;

    // The upper clamp KAPC_TIMEOUT_WEDGE_FRAME_NAMES is indexed with. The console
    // asserts the raw index against KI_NUM_TIMEOUT_WEDGE_FRAMES (5) and then clamps to
    // [0, 4] anyway (`cmpwi r29, 4` / `li r10, 4` @0x8242A0CC-@0x8242A0D4).
    const s32 KI_LAST_TIMEOUT_WEDGE_FRAME = 4;

    // X360 &unk_820046A7 -- the empty string every "blank this field" store passes.
    const char* const KPC_EMPTY_STRING = "";

    // The localisation ids this readout looks up. None of them appear as literals in
    // build/game/LANGUAGE/000*.bundle -- FindString hashes them (CgsHash ->
    // FindStringByHash), and an unknown id falls back to echoing the id text rather
    // than crashing, so a missing entry shows up as raw ids on the HUD.
    const char* const KPC_STUNTRUN_SCORE_FORMAT      = "STUNTRUN_SCORE_FORMAT";
    const char* const KPC_STUNTRUN_MULT_FORMAT       = "STUNTRUN_MULT_FORMAT";
    const char* const KPC_STUNTRUN_TRICK_FORMAT      = "STUNTRUN_TRICK_FORMAT";
    const char* const KPC_STUNTRUN_BOOSTTRICK_FORMAT = "STUNTRUN_BOOSTTRICK_FORMAT";
    const char* const KPC_ONLINE_SR_HUD_ELIMINATED   = "$ONLINE_SR_HUD_ELIMINATED";

    // The stunt bits the readout tests out of GuiCache's id-428 stunt masks. Bit N of
    // muCurrentStunts / muAllStunts is stunt type N, so the two literals the console
    // carries -- 0x100 (`rlwinm r11, r11, 0,23,23` @0x8242A62C) and 0x4000
    // (`rlwinm r11, r11, 0,17,17` @0x8242A6AC) -- are exactly these two shifts.
    const u32 KU_STUNT_MASK_BOOST =
        1u << static_cast<u32>(BrnGameState::E_STUNT_TYPE_BOOST);            // 0x00000100
    const u32 KU_STUNT_MASK_REVERSE_TAKEOFF =
        1u << static_cast<u32>(BrnGameState::E_STUNT_TYPE_REVERSE_TAKEOFF);  // 0x00004000

    // BrnMath::IntRound, inlined by the console at @0x8242A2EC-@0x8242A360 as the
    // classic add-then-subtract-2^52 floor trick: fsel picks +/-4503599627370496.0
    // (dbl_82001CB0 / dbl_82001CB8) by the sign of the input, the round-trip snaps to
    // the nearest integer, and a second fsel pair (dbl_82001CA0 == 1.0, dbl_82001CA8 ==
    // 0.0) subtracts the one-off when that landed above the input -- i.e. floor(). It is
    // applied to (value + 0.5f), so the whole thing is round-half-up.
    // [FLAG no BrnMath home] DELETE-WHEN a BrnMath TU lands IntRound and this becomes a
    // real call to it.
    s32 IntRound(f32 lfValue)
    {
        return static_cast<s32>(std::floor(lfValue + KF_INT_ROUND_BIAS));
    }
}

// ---------------------------------------------------------------------------
// @0x82435430 (decl BrnEventInfo.h:127; asserts at BrnEventInfo.cpp:759) -- the
// per-frame dispatch. Latch-gated: the arm only runs while the cache's game mode still
// agrees with the mode this component was told to display, so a mode change silences the
// panel until SetEventType @0x8242FC78 re-latches it.
//
// The console's 18-case jump table (jpt_824354A8) has NO arm for modes 1, 4, 6, 9, 11 or
// 13 -- FACE_OFF, PURSUIT, ELIMINATOR, TRAFFIC_ATTACK, ONLINE_ROAD_RAGE and
// ONLINE_BURNING_HOME_RUN all fall through to the default and draw nothing, even though
// the DecFIGS DWARF still declares UpdateFaceOff / UpdatePursuit / UpdateEliminator /
// UpdateTrafficAttack. They are dead in retail; do not add arms for them.
//
// Caller: RaceMainHudState::UpdateRunning @0x8247E898 (gated on its mbEventInfo byte).
// ---------------------------------------------------------------------------
void EventInfoComponent::Update(GuiCache* lpCache)
{
    CGS_ASSERT(lpCache != 0, "lpCache");   // BrnEventInfo.cpp:759 (non-gating)

    // `lwz r11, 0x3D4(r30)` then `lwzx r10, r31, 0x9E58` -- the cache's mode against the
    // latched one. GetGameMode() is the public accessor for the far member.
    if (lpCache->GetGameMode() != meCurrentEventType)
    {
        return;
    }

    switch (meCurrentEventType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:        // 0
            UpdateRace(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:    // 2
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:     // 16
            UpdateCrash(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:           // 3
            UpdateRoadRage(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE:       // 5
            UpdateBurningRoute(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:        // 7
            UpdateStuntAttack(lpCache, false);                            // li r5, 0
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:          // 8
            UpdateSurvivor(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_RACE:         // 10
            UpdateOnlineRace(lpCache);
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:     // 12
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:    // 14
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:     // 17
            UpdateStuntAttack(lpCache, true);                             // li r5, 1
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY: // 15
            UpdateFreeBurnLobby(lpCache);
            break;

        default:
            // jpt_824354A8 default case, cases 1, 4, 6, 9, 11, 13.
            break;
    }
}

// ---------------------------------------------------------------------------
// @0x82429C08 (decl BrnEventInfo.h:194) -- the stunt-run / stunt-attack readout: nine
// blocks, run every frame while the panel is showing mode 7 (offline) or 12 / 14 / 17
// (online).
//
//   1. mode-frame latch    -- pick the event-state animation and re-move the panel on a change
//   2. target score        -> maTextField[0]
//   3. current score       -> maTextField[1]
//   4. add-score delta     -> maTextField[6]   (ONLINE only)
//   5. the clock           -> maTextField[2]   (+ danger colour + flash animator)
//   6. eliminated banner   -> maTextField[2]   (ONLINE only)
//   7. combo-warning wedge -> mScoreAnimator / mMultiplierAnimator / the banking hand-off
//   8. combo + multiplier  -> maTextField[3] / maTextField[4] / mBankScoreTextField
//   9. the stunt name      -> maTextField[5] -> mAddScoreTextField
//
// Every animator state name below is verbatim from the image.
// ---------------------------------------------------------------------------
void EventInfoComponent::UpdateStuntAttack(GuiCache* lpCache, bool lbOnline)
{
    // The X360 prologue: r30 (the stunt cursor), r16 / r19 (the two "did it just start"
    // edge flags) and r29 (the event state, seeded to "idle") are all live across the
    // whole body.
    s32  liStuntIndex        = 0;                        // r30
    bool lbComboStarted      = false;                    // r16 -- IDA's v10
    bool lbMultiplierStarted = false;                    // r19 -- IDA's v11
    s32  liEventState        = KI_EVENT_STATE_IDLE;      // r29

    char lacNewText[KI_TEXT_BUFFER_SIZE];                // v96[128]
    char lacStuntScore[KI_STUNT_SCORE_BUFFER_SIZE];      // v97[64]

    // ---- 1. the mode-frame latch @0x82429C38-0x82429CD4 --------------------
    // Offline the state is always "idle"; online it names which team is ahead. Note the
    // asymmetry the asm carries and the pseudocode obscures: on the BLUE team the test is
    // `target <= current` (ble -> 2 else 5), off it the test is `target > current`
    // (bgt -> 4 else 3).
    if (lbOnline)
    {
        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpCache->GetPlayerActiveRaceCarIndex());

        if (lpCache->GetCurrentOnlinePlayerTeam(lePlayerActiveRaceCarIndex) == KI_PLAYER_TEAM_BLUE)
        {
            liEventState = (lpCache->GetTargetScoreInEvent() <= lpCache->GetCurrentScoreInEvent())
                         ? KI_EVENT_STATE_YOUR_BLUE_LEADS
                         : KI_EVENT_STATE_RIVAL_RED_LEADS;
        }
        else
        {
            liEventState = (lpCache->GetTargetScoreInEvent() > lpCache->GetCurrentScoreInEvent())
                         ? KI_EVENT_STATE_RIVAL_BLUE_LEADS
                         : KI_EVENT_STATE_YOUR_RED_LEADS;
        }
    }

    if (miCurrentEventStateIndex != liEventState)
    {
        // MoveAnimation re-binds mEventMovieClip and re-runs PrepareComponentsForGameMode,
        // so every text field below is resolved fresh after this call -- which is exactly
        // why the two cached scores are invalidated with it.
        MoveAnimation(KAPC_EVENT_STATE_NAMES[liEventState]);
        miCurrentEventStateIndex = liEventState;
        miTargetScoreInEvent     = -1;                   // stw r17, 0x42C
        miCurrentScoreInEvent    = -1;                   // stw r17, 0x430
    }

    // ---- 2. the target score -> maTextField[0] @0x82429CD4-0x82429D84 ------
    // The console reads miScoreTarget three times through the inlined getter and fires
    // its "-1 < miScoreTarget" assert (BrnGuiCache.h:3099) on the first two; the third
    // read, the one that feeds SPrintf and the cache, is unasserted. Restored as two
    // real GetTargetScoreInEvent() calls -- the getter carries the assert.
    const s32 liTargetScore = lpCache->GetTargetScoreInEvent();

    if (liTargetScore != miTargetScoreInEvent)
    {
        miTargetScoreInEvent = lpCache->GetTargetScoreInEvent();

        CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%d", miTargetScoreInEvent);
        maTextField[0].SetLocalisedText(KPC_STUNTRUN_SCORE_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP,
                                        1, lacNewText, KI_FORMAT_INTEGER);
    }

    // ---- 3. the current score -> maTextField[1] @0x82429D84-0x82429E1C -----
    // Same shape, "-1 < miScoreCurrent" (BrnGuiCache.h:3083).
    const s32 liCurrentScore = lpCache->GetCurrentScoreInEvent();

    if (liCurrentScore != miCurrentScoreInEvent)
    {
        miCurrentScoreInEvent = lpCache->GetCurrentScoreInEvent();

        CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%d", miCurrentScoreInEvent);
        maTextField[1].SetLocalisedText(KPC_STUNTRUN_SCORE_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP,
                                        1, lacNewText, KI_FORMAT_INTEGER);
    }

    // ---- 4. the add-score delta -> maTextField[6] @0x82429E1C-0x82429E70 ---
    // ONLINE only, and the seventh text field is the one the DWARF/PS3 build does not
    // have (maTextField is [7] on X360 -- see the header's layout note). The delta is
    // computed from the two CACHED scores, not from fresh cache reads.
    if (lbOnline)
    {
        const s32 liAddScore = miCurrentScoreInEvent - miTargetScoreInEvent;

        if (miAddScoreDisplayed != liAddScore)
        {
            CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%d", liAddScore);
            maTextField[6].SetLocalisedText(KPC_STUNTRUN_SCORE_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP,
                                            1, lacNewText, KI_FORMAT_INTEGER);
            miAddScoreDisplayed = liAddScore;
        }
    }

    // ---- 5. the clock -> maTextField[2] @0x82429E70-0x82429F9C -------------
    // mfEventTime is an f32 and BOTH reads here are `lfs` (@0x82429E80 / @0x82429EA8):
    // the inlined GetCurrentTimeInEvent() assert ("0.0f <= mfEventTime",
    // BrnGuiCache.h:2962) and the change test. The panel redraws only on a change.
    if (lpCache->GetCurrentTimeInEvent() != mfTimeLeft)
    {
        mfTimeLeft = lpCache->GetCurrentTimeInEvent();

        // `fmuls` by 1/60 then `fctiwz`, then `fnmsubs f0, (f32)minutes, 60.0f, time`
        // (== time - minutes*60) and another `fctiwz`. Both are TRUNCATIONS, not rounds.
        const s32 liMinutes = static_cast<s32>(mfTimeLeft * KF_ONE_OVER_SECONDS_PER_MINUTE);
        const s32 liSeconds = static_cast<s32>(mfTimeLeft
                                             - static_cast<f32>(liMinutes) * KF_SECONDS_PER_MINUTE);

        CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%dm%02ds", liMinutes, liSeconds);
        maTextField[2].SetText(lacNewText, false);

        SetTextFieldDangerColour(&maTextField[2], mfTimeLeft);

        if (mfTimeLeft >= KF_DANGERTIME_START)
        {
            if (mbTimeRemainingFlashing)
            {
                mTimeAnimatorStuntRun.Run("notFlashing");
                mbTimeRemainingFlashing = false;
            }
        }
        else if (!mbTimeRemainingFlashing)
        {
            mTimeAnimatorStuntRun.Run("flashing");
            mbTimeRemainingFlashing = true;
        }
    }

    // ---- 6. the eliminated banner -> maTextField[2] @0x82429F9C-0x8242A00C -
    // ONLINE only. Two independent sources: the cache's own elimination table, and the
    // online-player record's own flag. The record lookup is the X360's inlined
    // GetOnlinePlayerInfo()-by-active-race-car-index scan, restored as a real call.
    if (lbOnline)
    {
        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>(lpCache->GetPlayerActiveRaceCarIndex());

        bool lbEliminated = lpCache->IsOnlinePlayerEliminated(lePlayerActiveRaceCarIndex);

        if (!lbEliminated)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo = 0;

            for (s32 liPlayer = 0; liPlayer < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liPlayer)
            {
                const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpCandidate =
                    lpCache->GetOnlinePlayerInfo(liPlayer);

                if (lpCandidate->meActiveRaceCarIndex == lePlayerActiveRaceCarIndex)
                {
                    lpPlayerInfo = lpCandidate;
                    break;
                }
            }

            // [FLAG console-bug guard] The X360's not-found path does NOT bail: it leaves
            // r11 == r30 == 0 (`mr r11, r30` @0x82429FE8) and falls straight into
            // `lbz r11, 0x130(r11)` @0x82429FEC -- a read through a null record. The
            // scan cannot miss in practice (the local player is always in the table), so
            // the console never trips it. Guarded rather than reproduced.
            // DELETE-WHEN the record table is proven to be populated before this runs.
            //
            // Record +0x130 == 304 is the byte the console tests. The committed
            // InGamePlayerStatusData spells +304 as maReservedPadTo312[0] ("inert
            // padding") -- it is NOT inert; it is a real per-player flag with an
            // X360 reader. See conductor notes.
            lbEliminated = (lpPlayerInfo != 0) && (lpPlayerInfo->maReservedPadTo312[0] != 0);
        }

        if (lbEliminated)
        {
            maTextField[2].SetText(KPC_ONLINE_SR_HUD_ELIMINATED, false);
        }
    }

    // ---- 7. the combo-warning wedge @0x8242A00C-0x8242A20C ------------------
    // GuiCache::mbComboWarningActive is the cache's live flag; this component's
    // mbComboWarningActive is its own latched copy, so the block is a pure edge detector.
    if (lpCache->mbComboWarningActive)
    {
        if (!mbComboWarningActive)
        {
            // RISING EDGE. mfComboWarningTimeActive is an f32 loaded with `lfsx` and
            // TRUNCATED by `fctiwz` @0x8242A04C-0x8242A054 -- IDA prints an int read of
            // float memory here. See the banner note (2).
            const s32 liFrameNameIndex = static_cast<s32>(lpCache->mfComboWarningTimeActive);

            CGS_ASSERT(liFrameNameIndex >= 0,
                       "liFrameNameIndex >= 0");                                   // cpp:1439
            CGS_ASSERT(liFrameNameIndex <= KI_NUM_TIMEOUT_WEDGE_FRAMES,
                       "liFrameNameIndex <= KI_NUM_TIMEOUT_WEDGE_FRAMES");         // cpp:1440

            // Clamp<s32>(index, 0, KI_NUM_TIMEOUT_WEDGE_FRAMES - 1), inlined
            // @0x8242A0A4-0x8242A0D4. Note the asserted bound is 5 but the clamp is 4:
            // the console tolerates the off-by-one rather than reading past the table.
            s32 liClampedFrameNameIndex;
            if (liFrameNameIndex >= 0)
            {
                liClampedFrameNameIndex = (liFrameNameIndex > KI_LAST_TIMEOUT_WEDGE_FRAME)
                                        ? KI_LAST_TIMEOUT_WEDGE_FRAME
                                        : liFrameNameIndex;
            }
            else
            {
                liClampedFrameNameIndex = 0;
            }

            mbComboWarningActive = true;                                           // stb before the Run
            mScoreAnimator.Run(KAPC_TIMEOUT_WEDGE_FRAME_NAMES[liClampedFrameNameIndex]);

            if (miMultiplierInEvent > 1)
            {
                mMultiplierAnimator.Run("startpulse");
            }
        }
    }
    else if (mbComboWarningActive)
    {
        // FALLING EDGE. Either the combo BANKED (the cache's combo dropped below the
        // one on display) or it simply ran out.
        mbComboWarningActive = false;

        if (lpCache->GetCurrentComboInEvent() < miCurrentComboInEvent)
        {
            // BANKING. The banking animator's frame trigger calls
            // BankingTransitionCompleteCallback @0x824214C8, which is what clears
            // mbTottingUp again -- without it the tot-up latches on forever.
            mBankingAnimator.Run("banking");
            mScoreAnimator.Run("default");

            mbTottingUp = true;

            const f32 lfNow            = lpCache->GetTime();
            const s32 liBankedCombo    = miCurrentComboInEvent;   // lwz 0x438
            const s32 liBankedMultiplier = miMultiplierInEvent;   // lwz 0x43C

            // The ramp: the combo count counts UP to combo*multiplier while the
            // multiplier counts DOWN to 1. (`mullw r11, r11, r10` @0x8242A174 is the
            // finish score; the three `fcfid`/`frsp` pairs are the int->float casts.)
            mfTotupFinishMultiplier = 1.0f;                                        // stfs 0x45C
            mfTotupStartTime        = lfNow;                                       // stfs 0x460
            mfTotupStartScore       = static_cast<f32>(liBankedCombo);             // stfs 0x450
            mfTotupFinishScore      = static_cast<f32>(liBankedCombo * liBankedMultiplier); // stfs 0x454
            mfTotupStartMultiplier  = static_cast<f32>(liBankedMultiplier);        // stfs 0x458
        }
        else
        {
            // Timed out. The console re-reads both counters through their getters here.
            mScoreAnimator.Run((lpCache->GetCurrentComboInEvent() > 0) ? "endofincrease" : "default");
            mMultiplierAnimator.Run((lpCache->GetMultiplierInEvent() > 1) ? "endofincrease" : "default");
        }
    }

    // ---- 8. combo + multiplier @0x8242A20C-0x8242A53C ----------------------
    // "-1 < miScoreCombo" (BrnGuiCache.h:3115) / "-1 < miComboMultiplier"
    // (BrnGuiCache.h:3130) -- one assert each; the console's second load of each word is
    // unasserted, so one getter call each is the faithful count.
    const s32 liCurrentCombo      = lpCache->GetCurrentComboInEvent();
    const s32 liCurrentMultiplier = lpCache->GetMultiplierInEvent();

    if (mbTottingUp)
    {
        // The banking ramp. Two `fsel` pairs clamp the progression to [0, 1]; the two
        // lerps are `fmadds` (delta * t + start) and each is IntRound'ed.
        const f32 lfElapsed = lpCache->GetTime() - mfTotupStartTime;

        f32 lfProgression = (lfElapsed <= 0.0f) ? 0.0f : (lfElapsed / KF_TOTUP_DURATION);
        if (lfProgression > 1.0f)
        {
            lfProgression = 1.0f;
        }

        const s32 liTotupMultiplier = IntRound(mfTotupStartMultiplier
            + (mfTotupFinishMultiplier - mfTotupStartMultiplier) * lfProgression);
        const s32 liTotupScore = IntRound(mfTotupStartScore
            + (mfTotupFinishScore - mfTotupStartScore) * lfProgression);

        // The banked running total goes to its OWN field (mBankScoreTextField, X360
        // +0x7C), not to the combo field.
        mBankScoreTextField.SetLocalisedText(liTotupScore, KI_FORMAT_INTEGER);

        if (liTotupMultiplier <= 1)
        {
            maTextField[4].SetText(KPC_EMPTY_STRING, false);
            mMultiplierAnimator.Run("default");
        }
        else
        {
            CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%d", liTotupMultiplier);
            maTextField[4].SetLocalisedText(KPC_STUNTRUN_MULT_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP,
                                            1, lacNewText, KI_FORMAT_INTEGER);
        }
    }
    else if (liCurrentCombo != miCurrentComboInEvent || liCurrentMultiplier != miMultiplierInEvent)
    {
        // Something moved. Note the console does NOT write miCurrentComboInEvent when
        // nothing changed and does not write it at all on the tot-up path.
        if (miCurrentComboInEvent <= 0 && liCurrentCombo > 0)
        {
            lbComboStarted = true;
        }
        if (miMultiplierInEvent <= 1 && liCurrentMultiplier > 1)
        {
            lbMultiplierStarted = true;
        }

        // The score backing plate slides in when a combo starts and out when it ends.
        // (@0x8242A410-0x8242A43C: "transin" only on the 0 -> >0 edge; "transout" on any
        // frame the live combo is exactly 0; a NEGATIVE live combo drives neither.)
        if (liCurrentCombo > 0 && miCurrentComboInEvent <= 0)
        {
            mScoreBackgroundAnimator.Run("transin");
        }
        else if (liCurrentCombo == 0)
        {
            mScoreBackgroundAnimator.Run("transout");
        }

        // The multiplier walks ONE step per frame toward the live value (`addi r11, r11, 1`
        // @0x8242A454), but snaps straight back to 1 on the way down.
        if (liCurrentMultiplier > 1 && liCurrentMultiplier > miMultiplierInEvent)
        {
            miMultiplierInEvent = miMultiplierInEvent + 1;
            mMultiplierAnimator.Run(lbMultiplierStarted ? "transin" : "increase");
        }
        else if (liCurrentMultiplier < miMultiplierInEvent)
        {
            miMultiplierInEvent = 1;
            mMultiplierAnimator.Run("default");
        }

        miCurrentComboInEvent = liCurrentCombo;                                    // stw 0x438

        if (liCurrentCombo <= 0)
        {
            maTextField[3].SetText(KPC_EMPTY_STRING, false);
            maTextField[4].SetText(KPC_EMPTY_STRING, false);
        }
        else
        {
            CGS_ASSERT(miMultiplierInEvent >= 1, "miMultiplierInEvent >= 1");      // cpp:1594

            // The combo score goes through the INTEGER SetLocalisedText overload
            // (sub_8246CF18 @0x8246CF18) -- no string id, just the thousands-separated
            // number. The multiplier goes through the id-lookup varargs form.
            maTextField[3].SetLocalisedText(miCurrentComboInEvent, KI_FORMAT_INTEGER);

            if (miMultiplierInEvent <= 1)
            {
                maTextField[4].SetText(KPC_EMPTY_STRING, false);
            }
            else
            {
                CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%d", miMultiplierInEvent);
                maTextField[4].SetLocalisedText(KPC_STUNTRUN_MULT_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP,
                                                1, lacNewText, KI_FORMAT_INTEGER);
            }
        }
    }

    // ---- 9. the stunt name @0x8242A53C-0x8242A82C --------------------------
    // GetNumberOfStuntsToDisplay(), inlined @0x8242A548: walk the cache's -1-terminated
    // list. The console's loop is bounded at ONE (`cmpwi r28, 1 ; blt`), which is the
    // same bound GetStuntToDisplay @0x8240F770 carries -- the cache surfaces exactly one
    // stunt per frame.
    s32 liNumStuntsToDisplay = 0;
    {
        const StuntToDisplayInfo* lpCachedStunt = lpCache->maStuntToDisplay;
        do
        {
            if (lpCachedStunt->miStuntId == -1)
            {
                break;
            }
            ++liNumStuntsToDisplay;
            ++lpCachedStunt;
        }
        while (liNumStuntsToDisplay < 1);
    }

    if (liNumStuntsToDisplay > 0)
    {
        // Two `lwz`/`stw` pairs per entry -- the cache's {miStuntId, miField_04} record
        // is copied word-for-word into this component's {meStuntType, miStuntScore}.
        for (liStuntIndex = 0; liStuntIndex < liNumStuntsToDisplay; ++liStuntIndex)
        {
            const StuntToDisplayInfo* lpCachedStunt = lpCache->GetStuntToDisplay(liStuntIndex);

            maDisplayedStunt[liStuntIndex].meStuntType =
                static_cast<BrnGameState::EStuntType>(lpCachedStunt->miStuntId);
            maDisplayedStunt[liStuntIndex].miStuntScore = lpCachedStunt->miField_04;
        }
    }

    if (liStuntIndex < 1)
    {
        // Stamp the unfilled tail invalid (@0x8242A5A4). Only the TYPE word is written --
        // the console's `stw r17, 0(r10)` never touches the score half.
        s32 liPadIndex = liStuntIndex;
        do
        {
            maDisplayedStunt[liPadIndex].meStuntType = BrnGameState::E_STUNT_TYPE_INVALID;
            ++liPadIndex;
        }
        while (liPadIndex < 1);
    }

    const BrnGameState::EStuntType leStuntType   = maDisplayedStunt[0].meStuntType;
    const s32                      liStuntScore  = maDisplayedStunt[0].miStuntScore;

    if (leStuntType != BrnGameState::E_STUNT_TYPE_INVALID && liStuntScore != 0)
    {
        // The console's bound is EIGHTEEN (`cmplwi r30, 0x12` @0x8242A5E8) even though
        // its own assert text names BrnGameState::E_STUNT_TYPE_COUNT, which this tree
        // spells 15. The assert text is reproduced verbatim; the bound is the component's
        // own table size, so types 15..17 do NOT trip it.
        CGS_ASSERT(leStuntType >= 0 && leStuntType < KI_STUNT_TYPE_STRING_ID_COUNT,
                   "leStuntType >= 0 && leStuntType < BrnGameState::E_STUNT_TYPE_COUNT");  // cpp:1677

        CgsCore::SnPrintf(lacStuntScore, KI_STUNT_SCORE_PRINT_CAP, "%d", liStuntScore);

        if ((lpCache->muCurrentStunts & KU_STUNT_MASK_BOOST) != 0
            && leStuntType != BrnGameState::E_STUNT_TYPE_BOOST)
        {
            // Boosting THROUGH some other trick: "<BOOST> <trick> <score>". Three
            // parameter pairs -- see banner note (3) for where the third one hides.
            maTextField[5].SetLocalisedText(
                KPC_STUNTRUN_BOOSTTRICK_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP, 3,
                KAC_STUNT_TYPE_STRING_IDS[BrnGameState::E_STUNT_TYPE_BOOST], KI_FORMAT_TEXT_DATABASE_LOOKUP,
                KAC_STUNT_TYPE_STRING_IDS[leStuntType],                      KI_FORMAT_TEXT_DATABASE_LOOKUP,
                lacStuntScore,                                               KI_FORMAT_INTEGER);
        }
        else if ((lpCache->muAllStunts & KU_STUNT_MASK_REVERSE_TAKEOFF) != 0
                 && (leStuntType == BrnGameState::E_STUNT_TYPE_AIR
                     || leStuntType == BrnGameState::E_STUNT_TYPE_SPIN
                     || leStuntType == BrnGameState::E_STUNT_TYPE_BARREL_ROLL))
        {
            // A reverse take-off earlier in the run qualifies the air/spin/roll that
            // followed it: "<REVERSE TAKEOFF> <trick> <score>". The asm tests the three
            // types individually (`cmpwi r30, 2 / 0 / 1` @0x8242A6B8-0x8242A6CC), which
            // is what IDA collapses into `v76 <= 2`.
            maTextField[5].SetLocalisedText(
                KPC_STUNTRUN_BOOSTTRICK_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP, 3,
                KAC_STUNT_TYPE_STRING_IDS[BrnGameState::E_STUNT_TYPE_REVERSE_TAKEOFF], KI_FORMAT_TEXT_DATABASE_LOOKUP,
                KAC_STUNT_TYPE_STRING_IDS[leStuntType],                                KI_FORMAT_TEXT_DATABASE_LOOKUP,
                lacStuntScore,                                                         KI_FORMAT_INTEGER);
        }
        else
        {
            maTextField[5].SetLocalisedText(
                KPC_STUNTRUN_TRICK_FORMAT, KI_FORMAT_TEXT_DATABASE_LOOKUP, 2,
                KAC_STUNT_TYPE_STRING_IDS[leStuntType], KI_FORMAT_TEXT_DATABASE_LOOKUP,
                lacStuntScore,                          KI_FORMAT_INTEGER);
        }

        mStuntAnimator.Run("displaythenfade");
    }
    else
    {
        // Nothing left to display: retire whatever is still in the stunt field by handing
        // its text to the add-score field and flying that into the score. @0x8242A780.
        const char* lpcCurrentText = maTextField[5].GetText();

        // The console walks to the NUL and subtracts (@0x8242A790-0x8242A7A8) -- an
        // is-it-empty test spelled as a strlen.
        s32 liTextLength = 0;
        while (lpcCurrentText[liTextLength] != '\0')
        {
            ++liTextLength;
        }

        if (liTextLength != 0)
        {
            // GetText is called a SECOND time for the hand-off (@0x8242A7B8); the first
            // result is only used for the length test.
            mAddScoreTextField.SetText(maTextField[5].GetText(), false);
            mAddScoreAnimator.Run("moveToScore");

            // While the combo-warning wedge is up the score animator is already busy, so
            // the console leaves it alone.
            if (!mbComboWarningActive)
            {
                if (lbComboStarted)
                {
                    mScoreAnimator.Run("transin");
                }
                else if (miCurrentComboInEvent > 0)
                {
                    mScoreAnimator.Run("increase");
                }
            }
        }

        maTextField[5].SetText(KPC_EMPTY_STRING, false);
    }
}

// ---------------------------------------------------------------------------
// ⭐⭐⭐ @0x82412E98 (decl BrnEventInfo.h:214) -- THE SHOWTIME READOUT. Two fields, two
// change-latches, and that is the whole function: distance travelled into
// maTextField[0] and cars crashed into maTextField[1], each written only when the
// cache word differs from the copy this component is holding.
//
// ASM SPINE (0x82412E98..0x82412F18), in the console's order:
//   0x82412EBC  lfs   f0, 0x420(r31)          mfShowTimeDistanceTravelled (this)
//   0x82412EC0  lfsx  f1, r30, 0xA00C         GuiCache::mfShowTimeDistanceTravelled
//   0x82412EC4  fcmpu / beq                   -- a FLOAT compare (see the trap below)
//   0x82412ECC  li    r5, 0x11                E_FORMAT_SMALL_DISTANCE
//   0x82412ED0  stfs  f1, 0x420(r31)          latch, THEN write the field
//   0x82412ED4  addi  r3, r31, 0x1C           == &maTextField[0]  ((0x1C-0x1C)/0x0C == 0)
//   0x82412ED8  bl    sub_8246CE38            TextFieldRef::SetLocalisedText(f32, format)
//   0x82412EE0  lwz   r10, 0x41C(r31)         miShowTimeCarsCrashed (this)
//   0x82412EE8  lwzx  r4,  r30, 0xA004        GuiCache::miShowTimeCarsCrashed
//   0x82412EEC  cmpw  / beq
//   0x82412EF4  li    r5, 0xB                 E_FORMAT_INTEGER
//   0x82412EF8  stw   r4, 0x41C(r31)
//   0x82412EFC  addi  r3, r31, 0x28           == &maTextField[1]  ((0x28-0x1C)/0x0C == 1)
//   0x82412F00  bl    sub_8246CF18            TextFieldRef::SetLocalisedText(s32, format)
//
// ⚠️⚠️ THE IDA PSEUDOCODE GETS THE DISTANCE COMPARE WRONG. It prints
//     `if ( *(a2 + 40972) != *(result + 1056) )` -- an INT compare of two words, then an
// int store. The asm is `lfs`/`lfsx`/`fcmpu`/`stfs` and the value it hands
// SetLocalisedText rides f1, i.e. the FLOAT overload @0x8246CE38, not the int one
// @0x8246CF18. Transcribing the pseudocode would compare bit patterns and then feed a bit
// pattern to the small-distance formatter -- a field that renders and is wrong, which is
// worse than a blank one. Both members are declared f32/s32 in this component's own layout
// (mfShowTimeDistanceTravelled +0x420 / miShowTimeCarsCrashed +0x41C), so the types decide it.
//
// ⚠️ NO PrepareComponentsForGameMode ARM IS NEEDED AND NONE EXISTS. jpt_824291E4 routes
// modes 2 and 16 straight to the epilogue, so showtime gets ONLY the seven generic
// KAPC_TEXTFIELD_NAMES slots -- and maTextField[0] ("textField_1_mc") / maTextField[1]
// ("textField_2_mc") are exactly the two this body drives. The panel's frame is
// KAC_MODE_FRAME_NAMES[2] == KAC_MODE_FRAME_NAMES[16] == "ShowTime".
//
// ⓘ GuiCache::miShowTimeComboMultiplier (+0xA008) IS NOT READ HERE. RecEvent's case-434 arm
// stores it and nothing in this component looks at it; the console's showtime panel shows
// the crash count and the distance only. Do not add a third field.
// ---------------------------------------------------------------------------
void EventInfoComponent::UpdateCrash(GuiCache* lpCache)
{
    const f32 lfDistanceTravelled = lpCache->GetShowTimeDistanceTravelled();
    if (lfDistanceTravelled != mfShowTimeDistanceTravelled)
    {
        mfShowTimeDistanceTravelled = lfDistanceTravelled;
        maTextField[0].SetLocalisedText(lfDistanceTravelled, KI_FORMAT_SMALL_DISTANCE);
    }

    const s32 liCarsCrashed = lpCache->GetShowTimeCarsCrashed();
    if (liCarsCrashed != miShowTimeCarsCrashed)
    {
        miShowTimeCarsCrashed = liCarsCrashed;
        maTextField[1].SetLocalisedText(liCarsCrashed, KI_FORMAT_INTEGER);
    }
}

// ============================================================================
// [FLAG deferred 2026-08-27] the SIX remaining NON-STUNT per-mode Update arms. Update
// @0x82435430's jump table names all seven by symbol, so the link needs bodies
// the moment Update mounts -- but that wave's scope was the STUNT readout
// (case 7 -> UpdateStuntAttack, real above), and UpdateCrash landed 2026-08-29
// with the showtime score chain. Each remaining gate logs once and returns;
// entering one of those modes today shows an un-updated (blank) event panel,
// which the log line makes attributable. Console addresses for the follow-on
// wave (also in the BrnEventInfo.h declaration banner):
//   UpdateRace @0x8242FCF0 / UpdateRoadRage
//   @0x82429A48 / UpdateBurningRoute @0x8242A830 / UpdateSurvivor @0x82421530 /
//   UpdateOnlineRace @0x824296B0 / UpdateFreeBurnLobby @0x8242FE98.
// DELETE-WHEN (per arm): its real body lands in this TU family.
// ============================================================================
namespace
{
    void LogDeferredModeArm(const char* lpacArm)
    {
        static const char* sapcLogged[8] = {};
        for (int i = 0; i < 8; ++i)
        {
            if (sapcLogged[i] == lpacArm)
            {
                return;
            }
            if (sapcLogged[i] == 0)
            {
                sapcLogged[i] = lpacArm;
                break;
            }
        }
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[event-info] " << lpacArm
                << ": deferred per-mode arm (stunt wave gated it) [FLAG]\n";
        }
    }
}


void EventInfoComponent::UpdateOnlineRace(GuiCache*)   { LogDeferredModeArm("UpdateOnlineRace"); }
void EventInfoComponent::UpdateFreeBurnLobby(GuiCache*){ LogDeferredModeArm("UpdateFreeBurnLobby"); }



// ARTIST0x82412F20: the destination label is shared by checkpoint event panels.
void EventInfoComponent::UpdateDestinationText(GuiCache* lpCache)
{
    BrnGameState::LandmarkIndex landmark;
    if (lpCache->IsOnlineStartInProgress())
    {
        GuiTracker* tracker = lpCache->GetGuiTracker();
        CGS_ASSERT(tracker != 0, "lpGuiTracker");
        const s32 trackedIndex = tracker->GetCurrentlyTrackedIndex();
        if (trackedIndex < 0)
        {
            maTextField[0].ClearText();
            return;
        }
        landmark = lpCache->GetOnlineLandmarkIndex(static_cast<u32>(trackedIndex));
    }
    else
        landmark = lpCache->GetEventDestinationLandmarkIndex();
    if (landmark != mCurrentLandmark)
    {
        mCurrentLandmark = landmark;
        CGS_ASSERT(mCurrentLandmark != BrnGameState::LandmarkIndex(-1),
                   "BrnGameState::K_INVALID_LANDMARK != mCurrentLandmark");
        GuiEventUpdateSatNav::SatNavIconInfo info;
        lpCache->GetLandmarkInfoFromIndex(mCurrentLandmark, &info);
        char name[32];
        CgsCore::SnPrintf(name, sizeof(name), "LM_%llu", info.GetCgsId());
        maTextField[0].SetLocalisedText(name, KI_FORMAT_TEXT_DATABASE_LOOKUP);
    }
}

// ARTIST0x82421668. Position changes are held for at least one second.
void EventInfoComponent::SetPositionData(GuiCache* lpCache, TextFieldComponentType* lpPosition,
                                         TextFieldComponentType* lpTotalRacers)
{
    mfCurrentTimeSinceLastPositionChange += lpCache->GetTimeStep();
    if (lpCache->GetPlayerRacePosition() != miCurrentPosition && mfCurrentTimeSinceLastPositionChange > 1.0f)
    {
        mfCurrentTimeSinceLastPositionChange = 0.0f;
        miCurrentPosition = lpCache->GetPlayerRacePosition();
        lpPosition->SetLocalisedText(miCurrentPosition, KI_FORMAT_INTEGER);
        SetPositionTextState(miCurrentPosition);
    }
    if (lpCache->GetOpponentsInEvent() + 1 != miTotalRacers)
    {
        miTotalRacers = lpCache->GetOpponentsInEvent() + 1;
        lpTotalRacers->SetLocalisedText(miTotalRacers, KI_FORMAT_INTEGER);
    }
}

// ARTIST0x8242FCF0.
void EventInfoComponent::UpdateRace(GuiCache* lpCache)
{
    UpdateDestinationText(lpCache);
    if (lpCache->IsInShortcut() != mbInShortcut)
    {
        mbInShortcut = !mbInShortcut;
        mDistanceAnimatorRace.Run(mbInShortcut ? "inShortcut" : (mbNearFinish ? "flashing" : "notFlashing"));
    }
    const f32 distance = lpCache->GetDistanceInEvent();
    if (!mbInShortcut && distance != mfDistToCheckpoint)
    {
        if (!(distance > mfDistanceWarningThreshold) &&
            (mfDistToCheckpoint < 0.0f || !(mfDistanceWarningThreshold >= mfDistToCheckpoint)))
        {
            mbNearFinish = true;
            mDistanceAnimatorRace.Run("flashing");
        }
        else if (!(mfDistToCheckpoint > mfDistanceWarningThreshold) && !(distance <= mfDistanceWarningThreshold))
        {
            mbNearFinish = false;
            mDistanceAnimatorRace.Run("notFlashing");
        }
        mfDistToCheckpoint = distance;
        maTextField[4].SetLocalisedText(distance, KI_FORMAT_LARGE_DISTANCE);
        if (mbNearFinish)
        {
            const f32 proximity = 1.0f - distance * (mpStateInterface->IsUsingMetricUnits() ? 0.001f : 0.00062137097f);
            const f32 nonnegative = -proximity >= 0.0f ? 0.0f : proximity;
            mDistanceInterpolator.SetProportion(1.0f - nonnegative >= 0.0f ? nonnegative : 1.0f);
        }
    }
    SetPositionData(lpCache, &maTextField[2], &maTextField[3]);
}

// ARTIST0x82421530.
void EventInfoComponent::UpdateSurvivor(GuiCache* lpCache)
{
    UpdateDestinationText(lpCache);
    if (lpCache->GetDistanceInEvent() != mfRivalDistanceFromTarget)
    {
        mfRivalDistanceFromTarget = lpCache->GetDistanceInEvent();
        maTextField[2].SetLocalisedText(mfRivalDistanceFromTarget, KI_FORMAT_LARGE_DISTANCE);
    }
    if (lpCache->GetTargetTimeInEvent() != mfTargetTimeInEvent)
    {
        mfTargetTimeInEvent = lpCache->GetTargetTimeInEvent();
        maTextField[3].SetLocalisedText(static_cast<s32>(mfTargetTimeInEvent), KI_FORMAT_INTEGER);
    }
    if (lpCache->GetCurrentTimeInEvent() != mfCurrentTimeInEvent)
    {
        mfCurrentTimeInEvent = lpCache->GetCurrentTimeInEvent();
        maTextField[4].SetLocalisedText(static_cast<s32>(mfCurrentTimeInEvent), KI_FORMAT_INTEGER);
    }
}

// ARTIST0x8242A830.
void EventInfoComponent::UpdateBurningRoute(GuiCache* lpCache)
{
    UpdateDestinationText(lpCache);
    if (lpCache->GetDistanceInEvent() != mfRivalDistanceFromTarget)
    {
        mfRivalDistanceFromTarget = lpCache->GetDistanceInEvent();
        maTextField[2].SetLocalisedText(mfRivalDistanceFromTarget, KI_FORMAT_LARGE_DISTANCE);
    }
    char timeText[128];
    if (lpCache->GetTargetTimeInEvent() != mfTargetTimeInEvent)
    {
        mfTargetTimeInEvent = lpCache->GetTargetTimeInEvent();
        const s32 minutes = static_cast<s32>(mfTargetTimeInEvent * 0.016666668f);
        const s32 seconds = static_cast<s32>(mfTargetTimeInEvent - static_cast<f32>(minutes) * 60.0f);
        CgsCore::SPrintf(timeText, sizeof(timeText), "%dm%02ds", minutes, seconds);
        maTextField[3].SetText(timeText, false);
    }
    if (lpCache->GetCurrentTimeInEvent() != mfCurrentTimeInEvent)
    {
        const f32 currentTime = lpCache->GetCurrentTimeInEvent();
        mfCurrentTimeInEvent = mfTargetTimeInEvent - currentTime >= 0.0f ? currentTime : mfTargetTimeInEvent;
        const s32 minutes = static_cast<s32>(mfCurrentTimeInEvent * 0.016666668f);
        const s32 seconds = static_cast<s32>(mfCurrentTimeInEvent - static_cast<f32>(minutes) * 60.0f);
        CgsCore::SPrintf(timeText, sizeof(timeText), "%dm%02ds", minutes, seconds);
        maTextField[4].SetText(timeText, false);
        const f32 remaining = mfTargetTimeInEvent - mfCurrentTimeInEvent;
        SetTextFieldDangerColour(&maTextField[4], remaining);
        if (remaining < 10.0f && !mbTimeRemainingFlashing)
        {
            mTimeAnimatorBRoute.Run("flashing");
            mbTimeRemainingFlashing = true;
        }
        else if (remaining >= 10.0f && mbTimeRemainingFlashing)
        {
            mTimeAnimatorBRoute.Run("notFlashing");
            mbTimeRemainingFlashing = false;
        }
    }
}

}

// ============================================================================
// FOLDED FROM BrnEventInfo_wRR.cpp (wave RR) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gui/Flow/HUD/Components/BrnEventInfo_wRR.cpp
//
// BrnGui::EventInfoComponent -- THE ROAD RAGE READOUT. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (road-rage wave, 2026-09-02). Bodied here:
//
//   UpdateRoadRage          @0x82429A48   (DWARF BrnEventInfo.h:460) -- Update's case 3:
//                                         the event clock + the "N / target" takedown digits
//   SetTakedownsTextState   @0x82421758   (DWARF BrnEventInfo.h:368) -- Normal / Warning
//                                         text state off current-vs-target takedowns
//
// Partfile sibling of BrnEventInfo.cpp / _wS1 / _wS2 (one class, one header; must be
// mounted with them). Nothing here defines a .rdata table; every string below is a
// literal the X360 body loads directly.
//
// ---------------------------------------------------------------------------
// LAYOUT / CALLEE PINS (all from the asm @0x82429A48):
//   0x82429A9C  lfs  f13, 0x3FC(r31)   mfTimeLeft            (change test vs GuiCache::mfEventTime)
//   0x82429B14  addi r30, r31, 0x1C    maTextField[0]        (the clock; "%dm%02ds" via SetText)
//   0x82429B44  lbz  r11, 0x3E4(r31)   mbTimeRemainingFlashing
//   0x82429B5C  addi r3, r31, 0x1D8    mTimeAnimatorRRage    Run("flashing")     -- below 10 s
//   0x82429B7C  addi r3, r31, 0x210    mTimeAnimatorBRoute   Run("notFlashing")  -- at/above 10 s
//   0x82429B98  lwz  r11, 0x400(r31)   miCurrentTakedowns
//   0x82429BB4  addi r3, r31, 0x34     maTextField[2]        SetLocalisedText(current, 11)
//   0x82429BC8  lwz  r11, 0x404(r31)   miTargetTakedowns
//   0x82429BE4  addi r3, r31, 0x40     maTextField[3]        SetLocalisedText(target, 11)
//   0x82429BBC / 0x82429BEC  bl sub_8246CF18 == TextFieldRef::SetLocalisedText(s32, format)
//                                         (BrnFlaptTextFieldRef.cpp:137 assert, FormatText(s32),
//                                          SetText(buf, true)); `li r5, 0xB` == E_FORMAT_INTEGER.
//
// ⚠ THE ANIMATOR ASYMMETRY IS CONSOLE-FAITHFUL. The "flashing" edge runs the road-rage
//   time animator (+0x1D8) but the "notFlashing" edge runs the BURNING-ROUTE time animator
//   (+0x210) -- and PrepareComponentsForGameMode's road-rage arm (BrnEventInfo_wS1.cpp:309)
//   prepares only mTimeAnimatorRRage. FlaptAnimatorComponent::Run on an unprepared animator
//   iterates zero child clips (BrnGuiFlaptIconComponent.cpp:213), so on the console the
//   road-rage clock, once it starts flashing under 10 s, never visibly stops flashing when a
//   time extension lifts it back above 10 s (only mbTimeRemainingFlashing is cleared). The
//   sibling UpdateStuntAttack @0x82429F58-0x82429F98 drives ONE animator (+0x248) for both
//   edges; this arm does not. Transcribed as the binary has it -- retail behaviour is the
//   target, not a fix.
//
// The clock arithmetic is the same shape as UpdateStuntAttack's: `fmuls` by flt_820139F8
// (1/60) then `fctiwz` (TRUNCATION), then `fnmsubs f0, (f32)minutes, 60.0f, time` and a
// second `fctiwz`. Both conversions truncate; neither rounds.
//
// GuiCache::mfEventTime is read TWICE on the console -- once through the inlined
// GetCurrentTimeInEvent() (which carries the "0.0f <= mfEventTime" assert,
// BrnGuiCache.h:2962) for the change test, and once through the real call to refresh
// mfTimeLeft. Both are restored as getter calls; the getter carries the assert.
//
// NO "+1" FLASH STATE EXISTS ON THE CONSOLE (re-verified 2026-09-03, lane H). The
// takedown counter drives exactly two animators with exactly two states each:
//   SetTakedownsTextState   @0x82421758: +0x130 mTextStateAnimatorRRage
//                             "Warning" @0x82421884 (current < target, `cmpw ; bge`)
//                             "Normal"  @0x82421898 (current >= target)
//   SetTakedownsDigitsState @0x824218A8: +0x168 mTakedownNumbersAnimator
//                             "SingleDigit" @0x8242195C (current < 10, `cmpwi 0xA ; bge`)
//                             "MultipleDigits" @0x82421970
// The DWARF EventInfoComponent (BrnEventInfo.h:151-160) carries no other road-rage
// animator, and the only flashing pair ("flashing"/"notFlashing") belongs to the CLOCK.
// The per-takedown feedback the player sees is elsewhere: the "TAKEDOWN" HUD message
// (HudMessageAnalyzer::HandleTakedown @0x8251C3C0 -> HandleCrashedEvent @0x8251C7C0
// state 2) and the above-car banking score (AboveCarRenderer @0x824544D8).
//
// WITNESS: `[hud-rr] takedowns text <cur>/<target>` is logged after the two Apt text
// writes, on change only, first 8 occurrences ([FLAG PC witness]; delete when the HUD
// readout has been screenshot-verified against a console capture).
// ============================================================================

namespace BrnGui
{

namespace
{
// (fold: an identical definition of KF_DANGERTIME_START was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KF_ONE_OVER_SECONDS_PER_MINUTE was dropped here -- this TU defines it once, above)
// (fold: an identical definition of KF_SECONDS_PER_MINUTE was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KI_TEXT_BUFFER_SIZE was dropped here -- this TU defines it once, above)

// (fold: an identical definition of KI_FORMAT_INTEGER was dropped here -- this TU defines it once, above)

    // The takedown-counter text state SetTakedownsTextState drives on
    // mTextStateAnimatorRRage.
    const char* const KPC_TAKEDOWNS_STATE_NORMAL  = "Normal";
    const char* const KPC_TAKEDOWNS_STATE_WARNING = "Warning";

    // The clock's flashing states.
    const char* const KPC_TIME_FLASHING     = "flashing";
    const char* const KPC_TIME_NOT_FLASHING = "notFlashing";
}

// ---------------------------------------------------------------------------
// @0x82429A48 (DWARF BrnEventInfo.h:460) -- the road-rage event readout, called every
// frame by Update @0x82435430 for mode 3.
//
//   1. the clock          -> maTextField[0]   (only on a change of mfEventTime)
//   2. current takedowns  -> maTextField[2]   (only on a change)
//   3. target takedowns   -> maTextField[3]   (only on a change)
//   4. text state + digit layout, unconditionally.
// ---------------------------------------------------------------------------
void EventInfoComponent::UpdateRoadRage(GuiCache* lpCache)
{
    char lacNewText[KI_TEXT_BUFFER_SIZE];

    // ---- 1. the clock @0x82429A58-0x82429B8C -------------------------------
    if (lpCache->GetCurrentTimeInEvent() != mfTimeLeft)
    {
        mfTimeLeft = lpCache->GetCurrentTimeInEvent();

        const s32 liMinutes = static_cast<s32>(mfTimeLeft * KF_ONE_OVER_SECONDS_PER_MINUTE);
        const s32 liSeconds = static_cast<s32>(mfTimeLeft
                                             - static_cast<f32>(liMinutes) * KF_SECONDS_PER_MINUTE);

        CgsCore::SPrintf(lacNewText, KI_TEXT_BUFFER_SIZE, "%dm%02ds", liMinutes, liSeconds);
        maTextField[0].SetText(lacNewText, false);

        SetTextFieldDangerColour(&maTextField[0], mfTimeLeft);

        if (mfTimeLeft >= KF_DANGERTIME_START)
        {
            if (mbTimeRemainingFlashing)
            {
                // +0x210 -- the burning-route animator, as the binary has it (see banner).
                mTimeAnimatorBRoute.Run(KPC_TIME_NOT_FLASHING);
                mbTimeRemainingFlashing = false;
            }
        }
        else if (!mbTimeRemainingFlashing)
        {
            mTimeAnimatorRRage.Run(KPC_TIME_FLASHING);                       // +0x1D8
            mbTimeRemainingFlashing = true;
        }
    }

    // ---- 2. current takedowns -> maTextField[2] @0x82429B90-0x82429BBC ---
    bool lbTakedownTextWritten = false;
    if (lpCache->GetCurrentTakedownsInEvent() != miCurrentTakedowns)
    {
        miCurrentTakedowns = lpCache->GetCurrentTakedownsInEvent();
        maTextField[2].SetLocalisedText(miCurrentTakedowns, KI_FORMAT_INTEGER);
        lbTakedownTextWritten = true;
    }

    // ---- 3. target takedowns -> maTextField[3] @0x82429BC0-0x82429BEC ----
    if (lpCache->GetTargetTakedownsInEvent() != miTargetTakedowns)
    {
        miTargetTakedowns = lpCache->GetTargetTakedownsInEvent();
        maTextField[3].SetLocalisedText(miTargetTakedowns, KI_FORMAT_INTEGER);
        lbTakedownTextWritten = true;
    }

    // [FLAG PC witness] the final Apt text write of the takedown readout. Fires only on
    // the frames the console itself rewrites a field (the two change tests above), and
    // at most 8 times per process. No console counterpart. DELETE-WHEN: the road-rage HUD
    // has been screenshot-verified against a console capture.
    if (lbTakedownTextWritten)
    {
        static s32 siWitnessLeft = 8;
        if (siWitnessLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            --siWitnessLeft;
            *CgsDev::Log::gpDebugPrint
                << "[hud-rr] takedowns text " << miCurrentTakedowns
                << "/" << miTargetTakedowns << " [FLAG PC witness]\n";
        }
    }

    // ---- 4. the two animator states, every frame @0x82429BF0-0x82429BFC --
    SetTakedownsTextState();
    SetTakedownsDigitsState();
}

// ---------------------------------------------------------------------------
// @0x82421758 (DWARF BrnEventInfo.h:368; asserts at BrnEventInfo.cpp:2201/2202) --
// pick the takedown counter's text state: "Normal" once the target is met,
// "Warning" while it is not. Drives mTextStateAnimatorRRage (+0x130,
// `addi r3, r31, 0x130` @0x82421868).
//
// The console's two asserts stream the offending value after the message
// ("Invalid takedown count provided - " << miCurrentTakedowns, via the StrStream
// helper sub_821F0E50); the sibling SetTakedownsDigitsState in BrnEventInfo.cpp keeps
// the message text only, and this body follows that convention.
// ---------------------------------------------------------------------------
void EventInfoComponent::SetTakedownsTextState()
{
    CGS_ASSERT(miCurrentTakedowns >= 0,
               "Invalid takedown count provided - ");
    CGS_ASSERT(miTargetTakedowns > 0,
               "Invalid takedown target provided - ");

    if (miCurrentTakedowns >= miTargetTakedowns)
    {
        mTextStateAnimatorRRage.Run(KPC_TAKEDOWNS_STATE_NORMAL);
    }
    else
    {
        mTextStateAnimatorRRage.Run(KPC_TAKEDOWNS_STATE_WARNING);
    }
}

}
