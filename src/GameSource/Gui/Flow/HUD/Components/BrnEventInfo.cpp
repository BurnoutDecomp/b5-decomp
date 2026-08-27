#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::IsUsingMetricUnits
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"               // MovieClipInstance::ResetTimeline

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
