#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

#include "BrnCommonTypes.h"                                                        // Vector2, Vector4
#include "GameShared/GameClasses/Core/CgsAssert.h"                                 // CGS_ASSERT
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                             // MovieClipRef::{GotoAndPlayLabel,FindChildMovieClipOnFrame,SetFrameTriggerCallback}
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"                        // MovieClipInstance::ResetTimeline
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                             // TextFieldRef::{SetText,SetAutoSize,SetColour}
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponentUtils.h"  // TryFindTextFieldFromMovieClip

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
