#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

#include <cmath>                                                  // std::floor (the inlined BrnMath::IntRound)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // the [event-info] deferred-arm log
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SPrintf / CgsCore::SnPrintf
#include "GameSource/BurnoutConstants.h"                           // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameSource/Gui/BrnGuiCache.h"                            // BrnGui::GuiCache (+ StuntToDisplayInfo)
#include "GameSource/GameState/BrnGameStateTypes.h"                // BrnGameState::EStuntType
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData

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

    // SetTextFieldDangerColour's ramp start, and the same threshold that gates the
    // timer's flashing animator (flt_82004A20, `fcmpu` @0x82429F58). DWARF names it
    // KF_DANGERTIME_START (BrnEventInfo.h).
    const f32 KF_DANGERTIME_START = 10.0f;                    // 0x41200000

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

void EventInfoComponent::UpdateRace(GuiCache*)         { LogDeferredModeArm("UpdateRace"); }
void EventInfoComponent::UpdateOnlineRace(GuiCache*)   { LogDeferredModeArm("UpdateOnlineRace"); }
void EventInfoComponent::UpdateFreeBurnLobby(GuiCache*){ LogDeferredModeArm("UpdateFreeBurnLobby"); }
void EventInfoComponent::UpdateBurningRoute(GuiCache*) { LogDeferredModeArm("UpdateBurningRoute"); }
void EventInfoComponent::UpdateSurvivor(GuiCache*)     { LogDeferredModeArm("UpdateSurvivor"); }

}
