#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // StuntModeScoring::IsComboInProgress

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp
// ============================================================================
// BrnGameState::HUDMessageLogic -- the object's lifecycle + per-frame entry points
// (Construct 0x8236F530 / Prepare 0x82366478 / PreWorldUpdate 0x82389248 /
// PostWorldUpdate 0x8239D998), the stunt-scorer notification pump
// (GenerateStuntMessage 0x82394DF8), the per-frame online stunt-run HUD message generators
// (X360 0x82394838 / 0x82394920 / 0x82394A78 / 0x82394B88 / 0x82394CC0) plus the
// per-car team-change recorder (X360 0x8231E498). Reconstructed store-for-store from
// the X360 pseudocode + assembly; the scoring queries go through the committed
// ScoringSystem / CarData / CgsSystem::Time / BitArray APIs by name.
//
// ⭐ GenerateStuntMessage is the image's ONLY consumer of StuntModeScoring's one-shot
// mbRecentStunt / mbRecentCombo latches. Its absence is what made
// StuntModeScoring::UpdateBufferedScore's opening `CGS_ASSERT(!mbRecentStunt)` fire mid-run
// in an offline stunt race -- the latch was armed every banked stunt and never drained.
// See the body for the full console attestation.
//
// The X360 binary reads the ScoringSystem's mode-timer fields raw (mStartTime.miSeconds /
// mEndTime.miSeconds); those reads are the inlined IsTimeLimitActive() predicate, restored
// here as the call (BrnScoringSystem_Timer.cpp documents the same lowering). The
// `lpCarData != NULL` asserts fire at BrnHUDMessageLogic.cpp:1026 / :1120 / :1120 in the
// X360 build; preserved via CGS_ASSERT.
namespace BrnGameState
{
namespace
{
    // The online stunt-run mode duration (X360 read-only float at 0x82CDB7B4, also stored
    // into GameModeParams::mfModeTimeLimit by OnlineStuntRunMode::Start @ 0x82339E70). Used
    // by the "leading" generator to gate on at-least-ten-seconds-elapsed. Modelled as a
    // single named constant so the elapsed-time math is self-consistent (the comparison is
    // exact for any value of the limit). FLAG: the literal float byte value is not in the
    // per-function IDA exports; 120.0f is the inferred online stunt-run mode length.
    const f32 KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT = 120.0f;

    // The generator gates (X360 read-only floats). Recognisable round constants from the
    // comparison context: elapsed > 10s before reporting a new leader; combo warning while
    // remaining in (0, lfComboWarningTime]; the 30-second time warning fires while remaining
    // is below 31s and the warning has not already been announced at 30s.
    const f32 KF_LEADING_MIN_ELAPSED_SECONDS = 10.0f;   // 0x8202AC38
    const f32 KF_ZERO_SECONDS                = 0.0f;    // 0x82001CC0
    const f32 KF_TIME_WARNING_WINDOW_SECONDS = 31.0f;   // 0x820323F4
    const f32 KF_TIME_WARNING_SECONDS        = 30.0f;   // 0x82029F30
}

// ============================================================================
// Lifecycle + per-frame entry points.
// ============================================================================

// X360 0x8236F530. Binds the action queue's buffer, seeds the latched mode type and runs
// Prepare(). Called by ModeManager::Construct (console 0x82340008 `bl HUDMessageLogic::Construct`).
//
// X360 (0x8236F530), store for store:
//   bl VariableEventQueue<256,16>::Construct   ; the queue IS this object's first member (offset 0)
//   bl VariableEventQueue<256,16>::Prepare
//   std r10(0), 0x240(r31)  (twice)            ; the team-changed bit set
//   stw {7,6,5,4,3,2,1,0,8}, 0x190..0x1B0(r31) ; a nine-entry s32 table (written TWICE)
//   std r10(0), 0x1B8(r31)                     ; an 8-byte zero
//   stw r28(-1), 0x1C0(r31)                    ; meCurrentGameModeType = E_MODE_NONE
//   bl HUDMessageLogic::Prepare
//
// [X] NOT REPRODUCED, named rather than faked: the nine-entry table at +0x190..+0x1B0 and the
// 8-byte zero at +0x1B8. Neither offset is modelled by this file's semantic-parity layout (see
// the header's LAYOUT NOTE), and inventing members to hold them would be exactly the fabricated
// offset the tree forbids. The table's contents {7,6,5,4,3,2,1,0,8} read as a message-priority
// ordering over the nine EPlayerTeam slots; its only readers are the HUD generators this build
// does not mount. DELETE-WHEN those generators land and name the members.
void HUDMessageLogic::Construct()
{
    mActionQueue.Construct();
    mActionQueue.Prepare();

    mTeamChangedBits.Prepare();                                 // std 0, 0x240
    meCurrentGameModeType = GameStateModuleIO::E_MODE_NONE;     // stw -1, 0x1C0

    Prepare();
}

// X360 0x82366478. Re-seeds the message edge-trackers so each notification fires once per mode.
// Called by Construct and by PostWorldUpdate on every latched-mode change.
//
// The console writes twenty-five fields; the six below are every one of them that this file's
// semantic-parity layout models, and each is X360-proven:
//   *(a1+452) = 0.0   -> mfTimeInMode                (+0x1C4)
//   *(a1+464) = -1    -> miScoreMessageRaceCarIndex  (+0x1D0)
//   *(a1+476) = -1    -> meEliminationRaceCarIndex   (+0x1DC)
//   *(a1+480) = 0     -> miLastLeadingTeam           (+0x1E0)
//   *(a1+484) = 0     -> miLastVictoryTeam           (+0x1E4)
//   *(a1+488) = -1.0  -> mfLastTimeWarningAnnounced  (+0x1E8)
//
// [X] NOT REPRODUCED, named rather than faked: the other nineteen stores -- +456 (-1), +460 (0),
// +496/+504/+560 (8-byte zeroes), +512/+513 (0), +516/+520 (5.0), +524/+528 (0.0), +532 and +540
// (CgsSystem::Time::SetFloatVal 1.5 / 7.5), +548 (1), +552/+584 (-1), +568/+588 (0), +592 (-1.0),
// +596 (0.0), the +440 8-byte zero and the +400..+432 table Construct also writes. Every one of
// them belongs to a HUD generator this build does not mount (the race / crash / rival / BHR
// message families); none is read by GenerateStuntMessage. Note the console's Prepare does NOT
// touch miScoreSampleThisFrame (+0x1D4) or miScoreSampleLastFrame (+0x1D8) -- that omission is
// faithful, not an oversight here.
void HUDMessageLogic::Prepare()
{
    mfTimeInMode               = 0.0f;                                  // +0x1C4
    miScoreMessageRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);  // +0x1D0
    meEliminationRaceCarIndex  = static_cast<EActiveRaceCarIndex>(-1);  // +0x1DC
    miLastLeadingTeam          = 0;                                     // +0x1E0
    miLastVictoryTeam          = 0;                                     // +0x1E4
    mfLastTimeWarningAnnounced = -1.0f;                                 // +0x1E8
}

// X360 0x82389248. The drain: bulk-append this frame's notifications into the module's outgoing
// game-action queue, then empty the local one. Its whole body is
//   CGS_ASSERT(lpOutputGameActionQueue, "lpOutputGameActionQueue != NULL")  (BrnHUDMessageLogic.cpp:116)
//   VariableEventQueue<13312,16>::Append<256,16>(a2, a1);
//   VariableEventQueue<256,16>::Clear(a1);
// `a1` is the object itself because mActionQueue is its first member; here it is named.
void HUDMessageLogic::PreWorldUpdate(GameStateModuleIO::GameActionQueue* lpOutputGameActionQueue)
{
    CGS_ASSERT(lpOutputGameActionQueue != NULL, "lpOutputGameActionQueue != NULL");

    lpOutputGameActionQueue->Append(mActionQueue);
    mActionQueue.Clear();
}

// X360 0x8239D998. Latch the game mode, tick the in-mode clock, run the per-mode generators.
//
// ⚠ THE ARGUMENTS ARE THE DEVIATION, NOT THE BODY. The console signature is ten arguments --
//   PostWorldUpdate(this, lpActiveRaceCarInterface, leGameModeType, lpModeManager, lpScoringSystem,
//                   lpRaceCarCrashEventQueue, lpVehicleOutputInterface, lpTakedownQueue, lfDelta,
//                   [sp+0x5C] mePlayerActiveRaceCarIndex, [sp+0x67] IsGameModeInProgress(mode))
// -- and every argument past lpScoringSystem exists only for the message families this build does
// not mount. The four carried here are exactly the ones the reproduced body reads, so both of the
// console's head asserts stay verbatim rather than being dropped with the arguments they guard.
//
// REPRODUCED, statement for statement:
//   CGS_ASSERT(a2, "lpActiveRaceCarInterface != NULL")  (BrnHUDMessageLogic.cpp:144)
//   CGS_ASSERT(a5, "lpScoringSystem != NULL")           (BrnHUDMessageLogic.cpp:145)
//   if (*(a1+448) != a3) { Prepare(a1); *(a1+448) = a3; }
//   *(a1+452) += a9;
//   switch (*(a1+448)) { case 7: GenerateStuntMessage(a1, a5); break;
//                        case 12/14/17: GenerateStuntMessage(a1, a5); ... break; }
// Note the switch tests the LATCHED member, not the incoming argument -- they differ only on the
// frame the mode changes, and the console reads the member. Faithfully kept.
//
// [X] NOT REPRODUCED, named rather than faked -- the other switch arms and the tail:
//   case 0/10  GenerateRaceModeMessages;            case 3  GenerateCriticalDamageMessage;
//   case 11    GenerateOnlineBlueTeamEscapingMessage + ...AreBehindYouMessage + ...LeaderMilestone;
//   case 12/14 (not 17) GenerateOnlineStuntRunVictoryMessages + ...LeadingMessages;
//   case 12/14/17 GenerateOnlineStuntRunEliminationMessages + ...TimeMessages + ...ScoreMessages;
//   case 13    GenerateBurningHomeRunMessages;      case 15 DetectOnlineCrashes +
//              RemoveCrashingMessagesForTakendownPlayers;
//   tail       GenerateOnlineTeamChangeMessages(a1, a7, a31).
// The five online stunt-run generators ARE bodied in this file, but every one of them needs the
// local player's EActiveRaceCarIndex and/or a CgsSystem::Time that arrive in the dropped stack
// arguments; wiring them would mean inventing those values. Behaviour cost on an OFFLINE stunt
// race -- the mode this leg exists for -- is zero: case 7 is the whole of its arm.
// DELETE-WHEN the console's full argument set is reachable (a real PostWorldInputBuffer exists and
// ModeManager::PostWorldUpdate becomes the live caller again).
void HUDMessageLogic::PostWorldUpdate(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    GameStateModuleIO::EGameModeType leGameModeType,
    ScoringSystem* lpScoringSystem,
    f32 lfDelta)
{
    CGS_ASSERT(lpActiveRaceCarInterface != NULL, "lpActiveRaceCarInterface != NULL");
    CGS_ASSERT(lpScoringSystem != NULL, "lpScoringSystem != NULL");

    if (meCurrentGameModeType != leGameModeType)
    {
        Prepare();
        meCurrentGameModeType = leGameModeType;
    }

    mfTimeInMode += lfDelta;

    switch (meCurrentGameModeType)
    {
        case GameStateModuleIO::E_MODE_STUNT_ATTACK:        // case 7
            GenerateStuntMessage(lpScoringSystem);
            break;

        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:     // case 12
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:    // case 14
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:     // case 17
            GenerateStuntMessage(lpScoringSystem);
            break;

        default:
            break;
    }
}

// X360 0x82394DF8. THE stunt scorer's notification pump -- and the ONLY consumer in the whole
// image of the three one-shot latches StuntModeScoring arms: mbRecentCombo (+0x64's sibling
// +0x80), mbRecentStunt (+0x64) and the time-up edge. Neither WasStuntRecentlyPerformed
// (0x82313280) nor WasComboRecentlyPerformed (0x823132D0) has a single direct xref in the image;
// both are reached from here, the first through the scorer's vtable slot +0x18
// (`lwz r11,0(r31) / lwz r11,0x18(r11) / bctrl` @0x82394EBC..0x82394ED0).
//
// ⛔ THIS IS WHY StuntModeScoring::UpdateBufferedScore CAN OPEN WITH `CGS_ASSERT(!mbRecentStunt)`.
// The console arms the latch inside UpdateBufferedScore during ModeManager::PostWorldUpdate's
// per-mode scorer fork (0x8234AD2C) and drains it, later in that same PostWorldUpdate, through
// HUDMessageLogic::PostWorldUpdate (0x8234B0E8) -> here. Set and consumed inside one frame, so
// the next frame's UpdateBufferedScore always finds it false. With this function absent the latch
// is write-only and that assert fires on the SECOND stunt banked in any offline stunt race.
//
// X360, branch for branch:
//   * the scorer is picked off the LATCHED mode (`lwz r11, 0x1C0(r30)`): 12/14/17 -> the online
//     scorer at lpScoringSystem+0x2620, anything else -> the offline one at +0x350. Both are
//     reached BY NAME here (GetOnlineStuntScorer / GetStuntScorer), never by offset.
//   * CGS_ASSERT(lpStuntModeScoring, "lpStuntModeScoring")   (BrnHUDMessageLogic.cpp:1228)
//   * the three queries are an ELSE-IF CHAIN: at most one notification is emitted per frame, and
//     the stunt query is not even called on a frame the combo query fires. That ordering is
//     load-bearing and is reproduced exactly -- see the note below.
//   * combo arm : AddEvent(record, 133, 12) then AddEvent(&miCurrentScore, 20, 4).
//   * stunt arm : score = (s32)mfComboScore * miComboMultiplier + miCurrentScore
//                 (`lfs 0x20 / fctiwz / lwz 0x24 / lwz 0x10 / mullw / add`), then
//                 AddEvent(stuntInfo, 132, 24) then AddEvent(&score, 20, 4).
//                 GetComboScore() IS that fctiwz truncation (BrnStuntModeScoring_Queries.cpp:366).
//   * time arm  : AddEvent(<uninitialised byte>, 134, 1).
//
// ⓘ ON THE ELSE-IF: a frame where BOTH mbRecentCombo and mbRecentStunt were armed would leave the
// stunt latch set and trip UpdateBufferedScore next frame. The console cannot reach that state --
// mbRecentCombo is armed by EndCombo (0x823215D8), which clears mbComboInProgress, and the
// UpdateBufferedScore path that arms mbRecentStunt asserts mbComboInProgress at
// BrnStuntModeScoring_UpdatePass.cpp:437 immediately after arming it. The two are mutually
// exclusive by that invariant, which is why this stays an else-if and does NOT become three
// independent drains: turning it into three would silence a real tripwire.
void HUDMessageLogic::GenerateStuntMessage(ScoringSystem* lpScoringSystem)
{
    const bool lbOnlineStuntFamily =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)  ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END);

    StuntModeScoring* lpStuntModeScoring = lbOnlineStuntFamily
                                             ? lpScoringSystem->GetOnlineStuntScorer()   // ss+0x2620
                                             : lpScoringSystem->GetStuntScorer();        // ss+0x350
    CGS_ASSERT(lpStuntModeScoring != NULL, "lpStuntModeScoring");

    ComboPerformedMessage lComboMessage;
    if (lpStuntModeScoring->WasComboRecentlyPerformed(&lComboMessage.miComboScore,
                                                      &lComboMessage.mbValidCombo,
                                                      &lComboMessage.mfComboTime))
    {
        ScoreUpdateMessage lScoreMessage;
        lScoreMessage.miScore = lpStuntModeScoring->GetCurrentScore();   // lwz 0x10

        mActionQueue.AddEvent(&lComboMessage, E_HUD_MESSAGE_COMBO_PERFORMED,
                              sizeof(ComboPerformedMessage));
        mActionQueue.AddEvent(&lScoreMessage, E_HUD_MESSAGE_SCORE_UPDATE,
                              sizeof(ScoreUpdateMessage));
        return;
    }

    StuntPerformedMessage lStuntMessage;
    if (lpStuntModeScoring->WasStuntRecentlyPerformed(&lStuntMessage.mStuntInfo))
    {
        ScoreUpdateMessage lScoreMessage;
        lScoreMessage.miScore = lpStuntModeScoring->GetComboScore()          // (s32)mfComboScore
                                  * lpStuntModeScoring->GetComboMultiplier() // miComboMultiplier
                              + lpStuntModeScoring->GetCurrentScore();       // miCurrentScore

        mActionQueue.AddEvent(&lStuntMessage, E_HUD_MESSAGE_STUNT_PERFORMED,
                              sizeof(StuntPerformedMessage));
        mActionQueue.AddEvent(&lScoreMessage, E_HUD_MESSAGE_SCORE_UPDATE,
                              sizeof(ScoreUpdateMessage));
        return;
    }

    if (lpStuntModeScoring->WasTimeRecentlyUp())
    {
        StuntTimeUpMessage lTimeUpMessage;
        mActionQueue.AddEvent(&lTimeUpMessage, E_HUD_MESSAGE_STUNT_TIME_UP,
                              sizeof(StuntTimeUpMessage));
    }
}

// X360 0x82394A78. When a pending "rival eliminated" car is recorded, build the
// team-eliminated notification: if the eliminated rival's whole team is now out (and the
// team had more than one player) report the team, otherwise report the individual rival's
// network id. Clears the pending slot afterwards so the message fires once.
void HUDMessageLogic::GenerateOnlineStuntRunEliminationMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex)
{
    // Suppress once a victory has been declared this round.
    if (miLastVictoryTeam != 0)
    {
        return;
    }

    const EActiveRaceCarIndex leEliminatedCar = meEliminationRaceCarIndex;
    if (leEliminatedCar == -1)
    {
        return;
    }

    const GameStateModuleIO::EPlayerTeam leEliminatedTeam =
        lpScoringSystem->GetPlayerTeam(leEliminatedCar);

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->IsTeamEliminated(static_cast<s32>(leEliminatedTeam))
        && lpScoringSystem->GetTeamPlayerCount(static_cast<s32>(leEliminatedTeam)) > 1)
    {
        // The whole team is out -- report the team.
        lMessage.miTeam = static_cast<s32>(leEliminatedTeam);
        lMessage.mNetworkPlayerID = -1;
    }
    else
    {
        // Report the individual eliminated rival.
        const CarData* lpCarData = lpScoringSystem->GetCarData(meEliminationRaceCarIndex);
        CGS_ASSERT(lpCarData != NULL, "lpCarData != NULL");

        lMessage.mNetworkPlayerID = lpCarData->GetNetworkPlayerID();
        lMessage.miTeam = 0;
    }

    lMessage.mbLocalPlayerIsSubject = (meEliminationRaceCarIndex == leLocalPlayerIndex);

    const GameStateModuleIO::EPlayerTeam leLocalTeam =
        lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex);
    meEliminationRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
    lMessage.mbLocalPlayerOnTeam = (static_cast<s32>(leLocalTeam) == static_cast<s32>(leEliminatedTeam));

    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_ELIMINATION, sizeof(StuntRunTeamMessage));
}

// X360 0x82394838. Once every other team is eliminated, declare the leading stunt team the
// victor (single-player team -> report that player, otherwise report the team). Latches the
// victory team into miLastVictoryTeam so it fires once.
void HUDMessageLogic::GenerateOnlineStuntRunVictoryMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex)
{
    if (miLastVictoryTeam != 0)
    {
        return;
    }

    const s32 liLeadingTeam = lpScoringSystem->GetLeadingStuntTeam(0);
    if (!lpScoringSystem->AreAllOtherTeamsEliminated(liLeadingTeam))
    {
        return;
    }

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->GetTeamPlayerCount(liLeadingTeam) == 1)
    {
        lMessage.miTeam = 0;
        lMessage.mbLocalPlayerOnTeam = false;
        lMessage.mbLocalPlayerIsSubject =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mNetworkPlayerID = lpScoringSystem->GetFirstTeamPlayer(liLeadingTeam);
    }
    else
    {
        lMessage.miTeam = liLeadingTeam;
        lMessage.mNetworkPlayerID = -1;
        lMessage.mbLocalPlayerIsSubject = false;
        lMessage.mbLocalPlayerOnTeam =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
    }

    miLastVictoryTeam = liLeadingTeam;
    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_VICTORY, sizeof(StuntRunTeamMessage));
}

// X360 0x82394920. While the timer is active and at least ten seconds into the event,
// announce a change in the leading stunt team (single-player team -> report that player,
// otherwise report the team). Latches miLastLeadingTeam so it fires only on a change.
void HUDMessageLogic::GenerateOnlineStuntRunLeadingMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex,
    const CgsSystem::Time& lCurrentTime)
{
    if (!lpScoringSystem->IsTimeLimitActive())
    {
        return;
    }

    const CgsSystem::Time lRemaining = lpScoringSystem->GetModeTimeRemaining(lCurrentTime);
    const f32 lfElapsed = KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT - lRemaining.GetFloatVal();

    if (lfElapsed <= KF_LEADING_MIN_ELAPSED_SECONDS || miLastVictoryTeam != 0)
    {
        return;
    }

    const s32 liLeadingTeam = lpScoringSystem->GetLeadingStuntTeam(0);
    if (liLeadingTeam == miLastLeadingTeam)
    {
        return;
    }

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->GetTeamPlayerCount(liLeadingTeam) == 1)
    {
        lMessage.miTeam = 0;
        lMessage.mbLocalPlayerOnTeam = false;
        lMessage.mbLocalPlayerIsSubject =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mNetworkPlayerID = lpScoringSystem->GetFirstTeamPlayer(liLeadingTeam);
    }
    else
    {
        lMessage.miTeam = liLeadingTeam;
        lMessage.mbLocalPlayerOnTeam =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mbLocalPlayerIsSubject = false;
        lMessage.mNetworkPlayerID = -1;
    }

    miLastLeadingTeam = liLeadingTeam;
    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_LEADING, sizeof(StuntRunTeamMessage));
}

// X360 0x82394B88. Two time-based notifications while the timer is active: (1) when the
// remaining time enters the combo-warning window and a combo is in progress, announce the
// combo is about to end; (2) when the remaining time drops below 31s, announce the 30-second
// warning once. Each announcement latches mfLastTimeWarningAnnounced.
void HUDMessageLogic::GenerateOnlineStuntRunTimeMessages(
    ScoringSystem* lpScoringSystem, const CgsSystem::Time& lCurrentTime, f32 lfComboWarningTime)
{
    if (!lpScoringSystem->IsTimeLimitActive())
    {
        return;
    }

    const CgsSystem::Time lRemaining = lpScoringSystem->GetModeTimeRemaining(lCurrentTime);
    const f32 lfRemainingSeconds = lRemaining.GetFloatVal();

    if (lfRemainingSeconds > KF_ZERO_SECONDS && lfRemainingSeconds <= lfComboWarningTime)
    {
        if (lpScoringSystem->GetOnlineStuntScorer()->IsComboInProgress())
        {
            StuntRunComboEndMessage lMessage;
            mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_COMBO_END, sizeof(StuntRunComboEndMessage));
            mfLastTimeWarningAnnounced = KF_ZERO_SECONDS;
        }
    }

    if (lfRemainingSeconds < KF_TIME_WARNING_WINDOW_SECONDS
        && mfLastTimeWarningAnnounced != KF_TIME_WARNING_SECONDS)
    {
        StuntRunTimeMessage lMessage;
        lMessage.mfWarningTimeSeconds = KF_TIME_WARNING_SECONDS;
        mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_TIME, sizeof(StuntRunTimeMessage));
        mfLastTimeWarningAnnounced = KF_TIME_WARNING_SECONDS;
    }
}

// X360 0x82394CC0. When the watched rival's stunt score crosses the 1,000,000-point
// milestone this frame, announce it with that rival's network id; then clear the watch slot.
void HUDMessageLogic::GenerateOnlineStuntRunScoreMessages(ScoringSystem* lpScoringSystem)
{
    const s32 KI_SCORE_MILESTONE = 1000000;

    const EActiveRaceCarIndex leWatchedCar = miScoreMessageRaceCarIndex;
    if (leWatchedCar == -1)
    {
        return;
    }

    if (miScoreSampleThisFrame >= KI_SCORE_MILESTONE && miScoreSampleLastFrame < KI_SCORE_MILESTONE)
    {
        const CarData* lpCarData = lpScoringSystem->GetCarData(leWatchedCar);
        CGS_ASSERT(lpCarData != NULL, "lpCarData != NULL");

        StuntRunScoreMessage lMessage;
        lMessage.mNetworkPlayerID = lpCarData->GetNetworkPlayerID();
        lMessage.miScore = KI_SCORE_MILESTONE;
        mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_SCORE, sizeof(StuntRunScoreMessage));
    }

    miScoreMessageRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
}

// X360 0x8231E498. Flag that the given active-race-car has changed team this round. The
// X360 inlines BitArray<8>::SetBit; its bounds guard fires the dynamic CgsBitArray.h:222
// "Index: N, Number of bits: 8" StrStream assert, reduced here to the static expression
// per the committed BrnGameStateSharedIO.cpp convention. (The X360 returns `this`; that is
// a calling-convention artifact, dropped for this void method.)
void HUDMessageLogic::OnlineTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    CGS_ASSERT(static_cast<u32>(leActiveRaceCarIndex) < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "luIndex < NUMBITS");

    mTeamChangedBits.SetBit(static_cast<u32>(leActiveRaceCarIndex));
}
}
