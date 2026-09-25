// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_wN3_01.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// The online handlers GameStateModule::ProcessGameEvents calls: the free-burn challenge
// messages (cases 162-164, 168, 169, 171, 172), a player leaving (cases 121, 123, 124, 129) and
// the user cancelling the current mode. Most are one-instruction forwards into the embedded
// ChallengeManager; the two remote handlers carry the ChallengeManager's inlined remote
// begin / trigger bodies, and SetPlayerDisconnected the Burning Home Run runner re-pick.
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateModule.h"      // GetPlayerActiveRaceCarIndex
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{

// Case 162, subtypes 0 and 2. Only the host begins the challenge; the begin always runs as the
// host, and lbRemote (1 for subtype 2) makes it announce the selection.
void ModeManager::HandleLocalStartFreeburnChallengeMessage(
        CgsID lChallengeID,
        GameStateModuleIO::GameActionQueue* lpActionQueue,
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput,
        bool lbIsHost,
        bool lbRemote)
{
    if (lbIsHost)
    {
        mChallengeManager.BeginChallenge(lChallengeID, lpActionQueue, lpActiveRaceCarOutput, true, lbRemote);
    }
}

// Case 163. A non-host arms the host's challenge for its next ChallengeManager tick.
void ModeManager::HandleRemoteStartFreeburnChallengeMessage(CgsID lChallengeID,
                                                            GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                            bool lbIsHost)
{
    (void)lpActionQueue;
    if (!lbIsHost)
    {
        mChallengeManager.RemoteBeginChallenge(lChallengeID);
    }
}

// Case 164. Unconditional: the trigger latch is set whatever the host flag says.
void ModeManager::HandleRemoteTriggeredFreeburnChallengeMessage(CgsID lChallengeID,
                                                                GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                                bool lbIsHost)
{
    (void)lpActionQueue;
    (void)lbIsHost;
    mChallengeManager.RemoteTriggerFreeburnChallenge(lChallengeID);
}

// Case 168. The host ends its own challenges; everyone else follows the host's result.
void ModeManager::HandleOnlineEndFreeburnChallengeMessage(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                          EChallengeStatus leChallengeStatus,
                                                          bool lbIsHost)
{
    if (!lbIsHost)
    {
        mChallengeManager.RemoteEndChallenge(lpActionQueue, leChallengeStatus);
    }
}

// Cases 162 (subtype 0) and 169.
void ModeManager::TriggerFreeburnChallenge(CgsID lChallengeID,
                                           GameStateModuleIO::GameActionQueue* lpActionQueue,
                                           bool lbIsHost)
{
    mChallengeManager.TriggerFreeburnChallenge(lChallengeID, lpActionQueue, lbIsHost);
}

// Case 169 and the rich-presence update. The ChallengeManager's query, inlined.
CgsID ModeManager::GetCurrentFreeburnChallengeID()
{
    return mChallengeManager.GetCurrentFreeburnChallengeID();
}

// Case 171.
void ModeManager::HandleSuccessUpdateEvent(const CgsSystem::TimerStatusInterface* lpTimerStatusInterface,
                                           const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpEvent)
{
    mChallengeManager.HandleSuccessUpdateEvent(lpTimerStatusInterface, lpEvent);
}

// Case 172.
void ModeManager::HandleChallengeSuccessEvent(const GameStateModuleIO::FburnChallengeSuccessEvent* lpEvent)
{
    mChallengeManager.HandleChallengeSuccessEvent(lpEvent);
}

// Cases 129 and 121: the challenge bookkeeping forgets the player, then the scoring system drops
// the player's Burnout Skillz.
void ModeManager::NetworkPlayerRemoved(BrnNetwork::NetworkPlayerID lPlayerID,
                                       GameStateModuleIO::GameActionQueue* lpActionQueue,
                                       bool lbIsHost)
{
    mChallengeManager.NetworkPlayerRemoved(lPlayerID, lpActionQueue, lbIsHost);
    mScoringSystem.ClearPlayersBurnoutSkillzData(lPlayerID);
}

// Case 121. A remote player dropped: mark the car disconnected (once), and in Burning Home Run,
// when the dropped player was the runner, hand the run to someone else.
void ModeManager::SetPlayerDisconnected(
        BrnNetwork::NetworkPlayerID lPlayerID,
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(lpActionQueue != NULL, "lpActionQueue");
    CGS_ASSERT(lPlayerID != -1, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    CarData* lpCarData = mScoringSystem.GetCarData(lPlayerID);
    if (lpCarData != NULL && !lpCarData->GetScoreData()->GetDisconnected())
    {
        mScoringSystem.SetPlayerDisconnected(lPlayerID);

        if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN &&
            lpCarData->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
        {
            mOnlineBurningHomeRun.PickNewBurningHomeRunRunner(lpActiveRaceCarOutput, lpActionQueue);
        }
    }
}

// Case 123: the local player lost the connection. The challenge manager drops out of any
// challenge and every player's Burnout Skillz are cleared.
void ModeManager::LocalPlayerDisconnected(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    mChallengeManager.Disconnected(lpActionQueue);
    mScoringSystem.ClearAllBurnoutSkillzData();
}

// Cases 123 / 124. Latches +0x94FB and records whether the cancel came during the
// intro in +0x94FC, then aborts the current mode's state machine. With no mode it does nothing.
void ModeManager::UserCancelCurrentMode()
{
    if (mpCurrentGameMode == NULL)
    {
        return;
    }

    mbHasTimedOut   = true;
    mbHasCrashedOut = (mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_INTRO);
    mpCurrentGameMode->SendEvent(E_GME_ABORT);
}

// In the lobby / showtime pair only: showtime (and the offline showtime value the
// console tests alongside it) buffers the score in the lobby's skillz manager, the lobby itself
// scores it through its slot-20 override with the local player's car. The challenge manager
// always sees the score, whatever the mode.
void ModeManager::ProcessNewRoadScore(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                      BrnStreetData::ChallengePlayerScoreEntry lScoreEntry,
                                      BrnStreetData::ScoreType leScoreType,
                                      CgsID lRoadID,
                                      BrnStreetData::ChallengeIndex lChallengeIndex)
{
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
        meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        CGS_ASSERT(mpGameStateModule != NULL, "mpGameStateModule");

        if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
            meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            mOnlineFreeBurnLobby.BufferNewRoadScore(lScoreEntry, leScoreType, lChallengeIndex);
        }
        else
        {
            mOnlineFreeBurnLobby.ProcessNewRoadScore(lpOutputBuffer, lScoreEntry, leScoreType, lChallengeIndex,
                                                     mpGameStateModule->GetPlayerActiveRaceCarIndex());
        }
    }

    mChallengeManager.HandleRoadRuleScore(lScoreEntry, leScoreType, lRoadID);
}

}  // namespace BrnGameState
