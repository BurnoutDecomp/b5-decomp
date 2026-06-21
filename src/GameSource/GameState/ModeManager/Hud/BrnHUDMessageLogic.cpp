#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // StuntModeScoring::IsComboInProgress

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp
// ============================================================================
// BrnGameState::HUDMessageLogic -- per-frame online stunt-run HUD message generators
// (X360 0x82394838 / 0x82394920 / 0x82394A78 / 0x82394B88 / 0x82394CC0) plus the
// per-car team-change recorder (X360 0x8231E498). Reconstructed store-for-store from
// the X360 pseudocode + assembly; the scoring queries go through the committed
// ScoringSystem / CarData / CgsSystem::Time / BitArray APIs by name.
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
