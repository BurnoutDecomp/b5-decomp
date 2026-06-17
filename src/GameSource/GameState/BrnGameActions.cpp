#include "GameSource/GameState/BrnGameActions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>   // std::fabs

namespace
{
// SoundTriggerAction::IsEmpty query-position tolerance. The X360 reads this float from rodata
// (0x82029BA4), which is not in the available exports; this is an UNCONFIRMED stand-in -- the
// |lane| > eps emptiness test is faithful, only the literal value is provisional.
const float KF_QUERY_POS_EPSILON = 1.0e-4f;
}

// CgsNetwork::K_INVALID_PLAYER_ID has no committed home (it lives in the un-reconstructed
// CgsNetworkConstants.h); modelled file-local as -1, exactly as the committed
// BrnGameStateFlybyManager.cpp does. The assert expression strings keep the original spelling.
namespace CgsNetwork
{
static const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// File-local debug-print plumbing for RankInfoResponseAction::SetProgressionRankEventWins (the
// X360 filter-gated rank-win spew), modelled as in the committed BrnRaceMode.cpp / BrnGuiModule.cpp:
// gpDebugPrint is a chained line-writer (vtable slot 1 writes a C-string); WriteInt writes an int and
// returns the stream. extern -> resolved at link, not needed for the compile gate.
namespace CgsDev
{
    namespace Message { extern u32 gxMessageFilterFlags; }
    namespace Log
    {
        struct DebugPrint;
        typedef DebugPrint* (*DebugPrintFn)(DebugPrint*, const char*);
        struct DebugPrint { DebugPrintFn* mpVTable; };
        extern DebugPrint* gpDebugPrint;
        extern DebugPrint* WriteInt(DebugPrint* lpStream, int liValue);
    }
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x822A0250. True iff the requested reset position OR direction is non-zero. The X360
// spelled this with the rwmath VPU operator!=(Vector3,Vector3); the unused 4th (w) lane is zero
// in both operands, so this is a faithful 3-lane (V3) compare.
bool ResetPlayerCarAction::HasToChangeLocation() const
{
    if (mPosition.x != 0.0f || mPosition.y != 0.0f || mPosition.z != 0.0f)
    {
        return true;
    }
    return mDirection.x != 0.0f || mDirection.y != 0.0f || mDirection.z != 0.0f;
}

// X360 0x82355178. Empty iff the query position is (within tolerance) zero AND there is no
// entity, no result type, and no active-trigger bits.
bool SoundTriggerAction::IsEmpty()
{
    if (std::fabs(mQueryPos.x) > KF_QUERY_POS_EPSILON
        || std::fabs(mQueryPos.y) > KF_QUERY_POS_EPSILON
        || std::fabs(mQueryPos.z) > KF_QUERY_POS_EPSILON)
    {
        return false;
    }
    if (mEntityId.muValue != 0u)
    {
        return false;
    }
    if (meResultType != E_TYPE_INVALID)
    {
        return false;
    }
    if (muActiveTriggers != 0u)
    {
        return false;
    }
    return true;
}

// X360 0x8230FE98. Range-checked setter for the disconnected player's active-race-car slot.
void RemotePlayerDisconnectedAction::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "( leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

    meActiveRaceCarIndex = leActiveRaceCarIndex;
}

// X360 0x8230FF00. Store the disconnected player's network id (must not be the invalid sentinel).
void RemotePlayerDisconnectedAction::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    mPlayerID = lPlayerID;
}

// X360 0x82355088. Initialise a setup-network-car action payload (6 members). The three asserts
// bound the race-car index and reject the null model id (expression/file/line byte-exact).
void SetupNetworkCarAction::Construct(EPlayerScoringIndex lePlayerScoringIndex,
                                      EActiveRaceCarIndex leActiveRaceCarIndex,
                                      Vector3             lPos,
                                      Vector3             lAt,
                                      CgsID               lModelId,
                                      CgsID               lWheelModelId)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(lModelId != 0, "lModelId != kCGSID_NULL");

    mWorldSpacePosition  = lPos;
    mAt                  = lAt;
    mModelId             = lModelId;
    mWheelModelId        = lWheelModelId;
    meActiveRaceCarIndex = leActiveRaceCarIndex;
    mePlayerScoringIndex = lePlayerScoringIndex;
}

// X360 0x823551F0. Bounds-checked setter for the added player's scoring slot.
void OnlinePlayerAddedAction::SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex)
{
    CGS_ASSERT(
        (lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) &&
        (lePlayerScoringIndex <  E_PLAYER_SCORING_INDEX_COUNT),
        "(lePlayerScoringIndex >= E_PLAYER_SCORING_INDEX_0) && (lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT)");

    mePlayerScoringIndex = lePlayerScoringIndex;
}

// X360 0x82355258. Range-checked setter for the removed online player's active-race-car slot.
// (The X360 streamed a dynamic "invalid race car index" message via StrStream; reduced here to a
// static CGS_ASSERT-style expression, matching the committed RemotePlayerDisconnectedAction.)
void OnlinePlayerRemovedAction::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "We have an invalid race car index here");

    meActiveRaceCarIndex = leActiveRaceCarIndex;
}

// X360 0x823554B0. Store the player's overall progression rank + the four per-mode ranks. The two
// asserts bound the rank below the rank count and reject the "finished last rank" sentinel
// (verbatim file/line). liRankCount is assert-only (not stored). Void; return-result dropped.
void RankInfoResponseAction::SetProgressionRanks(s32 liPlayerRank, s32 liRankCount, s32 liOfflineRace,
                                                 s32 liRoadRage, s32 liStuntAttack, s32 liMarkedMan)
{
    CGS_ASSERT(liPlayerRank < liRankCount, "liPlayerRank < liRankCount");
    CGS_ASSERT(liPlayerRank != KI_PLAYER_HAS_FINISHED_LAST_RANK, "liPlayerRank != KI_PLAYER_HAS_FINISHED_LAST_RANK");

    miPlayerRank  = liPlayerRank;
    miOfflineRace = liOfflineRace;
    miRoadRage    = liRoadRage;
    miStuntAttack = liStuntAttack;
    miMarkedMan   = liMarkedMan;
}

// X360 0x82355328. Store the four per-mode rank-win counts, after optionally echoing each through
// the global debug stream when the message filter's bit 0 is set. Void; return-result dropped.
void RankInfoResponseAction::SetProgressionRankEventWins(s32 liOfflineRaceRankWins, s32 liRoadRageRankWins,
                                                         s32 liStuntAttackRankWins, s32 liMarkedManRankWins)
{
    using namespace CgsDev;
    if ((Message::gxMessageFilterFlags & 1) != 0)
    {
        Log::DebugPrint* lp = Log::gpDebugPrint->mpVTable[1](Log::gpDebugPrint, "liOfflineRaceRankWins : ");
        lp = Log::WriteInt(Log::gpDebugPrint, liOfflineRaceRankWins); lp->mpVTable[1](lp, "\n");
        lp = Log::gpDebugPrint->mpVTable[1](Log::gpDebugPrint, "liRoadRageRankWins : ");
        lp = Log::WriteInt(Log::gpDebugPrint, liRoadRageRankWins);    lp->mpVTable[1](lp, "\n");
        lp = Log::gpDebugPrint->mpVTable[1](Log::gpDebugPrint, "liStuntAttackRankWins : ");
        lp = Log::WriteInt(Log::gpDebugPrint, liStuntAttackRankWins); lp->mpVTable[1](lp, "\n");
        lp = Log::gpDebugPrint->mpVTable[1](Log::gpDebugPrint, "liMarkedManRankWins : ");
        lp = Log::WriteInt(Log::gpDebugPrint, liMarkedManRankWins);   lp->mpVTable[1](lp, "\n");
    }

    miOfflineRaceRankWins = liOfflineRaceRankWins;
    miRoadRageRankWins    = liRoadRageRankWins;
    miStuntAttackRankWins = liStuntAttackRankWins;
    miMarkedManRankWins   = liMarkedManRankWins;
}

// X360 0x8230FDF0. Initialise a prepare-for-mode action payload: stamp the stage to "all in one",
// copy in the per-event GameModeParams, store the current round + "coming from online lobby" flag,
// and reset the per-player scoring slots + the disconnected-player table to their empty markers.
void PrepareForModeAction::Construct(const GameModeParams* lpGameModeParams,
                                     s32                    liCurrentRound,
                                     bool                   lbComingFromOnlineLobbyMode)
{
    CGS_ASSERT(lpGameModeParams, "lpGameModeParams");

    mePrepareForModeStage             = E_PFM_STAGE_ALL_IN_ONE;
    mGameModeParams                   = *lpGameModeParams;
    miCurrentRound                    = liCurrentRound;
    mbComingFromOnlineLobbyMode       = lbComingFromOnlineLobbyMode;
    mbFinishedOnlineEvent             = false;
    mbStartingFreeburnDueToPlayerJoin = false;
    miShotGroup                       = -1;

    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maePlayerScoringIndex[liIndex]         = E_PLAYER_SCORING_INDEX_COUNT;
        maDisconnectedNetworkPlayerID[liIndex] = CgsNetwork::K_INVALID_PLAYER_ID;
    }

    miNumPlayersDisconnected = 0;
}

// X360 0x822A0198. Linear search of the disconnected-player table: true iff lNetworkPlayerID is one
// of the miNumPlayersDisconnected recorded ids. The two asserts bound the count to [0, 8).
bool PrepareForModeAction::GetPlayerDisconnected(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const
{
    CGS_ASSERT(miNumPlayersDisconnected >= 0, "miNumPlayersDisconnected >= 0");
    CGS_ASSERT(miNumPlayersDisconnected < KI_MAX_DISCONNECTED_NETWORK_PLAYERS, "miNumPlayersDisconnected < BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    for (s32 liIndex = 0; liIndex < miNumPlayersDisconnected; ++liIndex)
    {
        if (maDisconnectedNetworkPlayerID[liIndex] == lNetworkPlayerID)
        {
            return true;
        }
    }
    return false;
}

// X360 0x8230FD60. Append a disconnected player's network id to the fixed-capacity disconnected-player
// list, then bump the count. The two asserts bound the count (>= 0 and < KI_MAX_ACTIVE_RACE_CARS - 1).
void PrepareForModeAction::SetPlayerDisconnected(BrnNetwork::NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(miNumPlayersDisconnected >= 0, "miNumPlayersDisconnected >= 0");
    CGS_ASSERT(miNumPlayersDisconnected < 7, "miNumPlayersDisconnected < (BrnWorld::KI_MAX_ACTIVE_RACE_CARS - 1)");

    maDisconnectedNetworkPlayerID[miNumPlayersDisconnected] = lPlayerID;
    ++miNumPlayersDisconnected;
}

// X360 0x8231CA38. Record a player's finishing slot. The valid-id assert rejects the invalid
// sentinel; position -1 means "disconnected" -> append the id to the disconnected list, else
// bounds-check the position and store the id at that slot. (X360 return-result artifact dropped.)
void OnlineRoundResults::SetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liPosition)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    if (liPosition == -1)
    {
        maDisconnectedPlayers.Append(lNetworkPlayerID);
    }
    else
    {
        CGS_ASSERT(liPosition >= 0, "liPosition >= 0");
        CGS_ASSERT(liPosition < KI_MAX_PLAYERS, "liPosition < KI_MAX_PLAYERS");

        maPlayerPosition[liPosition] = lNetworkPlayerID;
    }
}

// X360 0x82558580. Look up a player's finishing position. Disconnected players report
// KI_POSITION_DISCONNECTED; otherwise linear-scan the position table for the id and return its slot.
// Reaching the end without a match is a logic error (X360 streamed a runtime CgsStrStream message;
// reduced to a static CGS_ASSERT, still returning the -1 sentinel).
s32 OnlineRoundResults::GetPosition(BrnNetwork::NetworkPlayerID lNetworkPlayerID) const
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    for (s32 liIndex = 0; liIndex < maDisconnectedPlayers.GetCount(); ++liIndex)
    {
        if (maDisconnectedPlayers.GetItem(liIndex) == lNetworkPlayerID)
        {
            return KI_POSITION_DISCONNECTED;
        }
    }

    for (s32 liPosition = 0; liPosition < KI_MAX_PLAYERS; ++liPosition)
    {
        if (maPlayerPosition[liPosition] == lNetworkPlayerID)
        {
            return liPosition;
        }
    }

    CGS_ASSERT(false, "Player has no result position.");
    return KI_POSITION_DISCONNECTED;
}
}
}
