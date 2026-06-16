#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // Vector3, CgsID, EntityId
#include "GameSource/BurnoutConstants.h"                     // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerScoringIndex, EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID

// Owning header for the BrnGameState::GameStateModuleIO GameAction<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Each struct is a minimal slice: only the members the
// reconstructed body touches are declared. The GameAction<T> base is modelled as an empty
// template spine (the real build adds only a static type tag, no instance data/vtable); its real
// Event base + the per-action mseType definitions land with the full BrnGameActions TU.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// EGameActionType discriminant. Only the slots this batch instantiates are listed (the full enum
// is its own TU). The two unconfirmed values are placeholders used purely as template tags.
enum EGameActionType
{
    E_ACTION_RESET_PLAYER_CAR           = 0,
    E_ACTION_REMOTE_PLAYER_DISCONNECTED = 11,
    E_ACTION_SOUND_TRIGGER              = 210,
    E_ACTION_ONLINE_PLAYER_ADDED        = 220,   // value unconfirmed (template tag only)
    E_ACTION_SETUP_NETWORK_CAR          = 221,   // value unconfirmed (template tag only)
    E_ACTION_ONLINE_PLAYER_REMOVED      = 222,   // value unconfirmed (template tag only)
    E_ACTION_RANK_INFO_RESPONSE         = 173,   // DWARF BrnGameActions.h:183
};

template <EGameActionType T>
struct GameAction { };

// X360 0x822A0250 (HasToChangeLocation). Minimal slice: the two transform members it reads.
struct ResetPlayerCarAction : public GameAction<E_ACTION_RESET_PLAYER_CAR>
{
    Vector3 mPosition;
    Vector3 mDirection;

    bool HasToChangeLocation() const;
};

// X360 0x82355178 (IsEmpty).
struct SoundTriggerAction : public GameAction<E_ACTION_SOUND_TRIGGER>
{
    enum eType { E_TYPE_INVALID = 0, E_TYPE_AT_ENTITY = 1, E_TYPE_AHEAD_OF_ENTITY = 2, E_TYPE_COUNT = 3 };

    Vector3  mQueryPos;
    EntityId mEntityId;
    eType    meResultType;
    u32      muActiveTriggers;

    bool IsEmpty();
};

// X360 0x8230FE98 / 0x8230FF00.
struct RemotePlayerDisconnectedAction : public GameAction<E_ACTION_REMOTE_PLAYER_DISCONNECTED>
{
    EActiveRaceCarIndex         meActiveRaceCarIndex;
    BrnNetwork::NetworkPlayerID mPlayerID;

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lPlayerID);
};

// X360 0x82355088 (Construct). DWARF: 6 members / 0x38 bytes.
struct alignas(16) SetupNetworkCarAction : public GameAction<E_ACTION_SETUP_NETWORK_CAR>
{
    Vector3             mWorldSpacePosition;   // 0x00
    Vector3             mAt;                   // 0x10
    CgsID               mModelId;              // 0x20
    CgsID               mWheelModelId;         // 0x28
    EActiveRaceCarIndex meActiveRaceCarIndex;  // 0x30
    EPlayerScoringIndex mePlayerScoringIndex;  // 0x34

    void Construct(EPlayerScoringIndex lePlayerScoringIndex,
                   EActiveRaceCarIndex leActiveRaceCarIndex,
                   Vector3             lPos,
                   Vector3             lAt,
                   CgsID               lModelId,
                   CgsID               lWheelModelId);
};

// X360 0x823551F0 (SetPlayerScoringIndex). Layout per the Feb-2007 leak (this X360 build).
struct OnlinePlayerAddedAction : public GameAction<E_ACTION_ONLINE_PLAYER_ADDED>
{
    CgsID               mModelID;             // 0x00
    CgsID               mWheelID;             // 0x08
    EPlayerScoringIndex mePlayerScoringIndex; // 0x10
    EPlayerTeam         meTeam;               // 0x14

    void SetPlayerScoringIndex(EPlayerScoringIndex lePlayerScoringIndex);
};

// X360 0x82355258 (SetActiveRaceCarIndex). Minimal slice: only the member the body touches.
struct OnlinePlayerRemovedAction : public GameAction<E_ACTION_ONLINE_PLAYER_REMOVED>
{
    EActiveRaceCarIndex meActiveRaceCarIndex;   // 0x00
    bool                mbIsLocalPlayerInGame;  // 0x04 (DWARF BrnGameActions.h:4161; untouched by SetActiveRaceCarIndex)

    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);
};

// X360 0x823554B0 (SetProgressionRanks) / 0x82355328 (SetProgressionRankEventWins). The rank-info
// response action: the player's overall rank + per-mode ranks (0x00-0x10), then the per-mode
// rank-win counts (0x14-0x20).
struct RankInfoResponseAction : public GameAction<E_ACTION_RANK_INFO_RESPONSE>
{
    static const s32 KI_PLAYER_HAS_FINISHED_LAST_RANK = -1;

    s32 miPlayerRank;            // 0x00
    s32 miOfflineRace;           // 0x04
    s32 miRoadRage;              // 0x08
    s32 miStuntAttack;           // 0x0C
    s32 miMarkedMan;             // 0x10
    s32 miOfflineRaceRankWins;   // 0x14
    s32 miRoadRageRankWins;      // 0x18
    s32 miStuntAttackRankWins;   // 0x1C
    s32 miMarkedManRankWins;     // 0x20

    void SetProgressionRanks(s32 liPlayerRank, s32 liRankCount, s32 liOfflineRace,
                             s32 liRoadRage, s32 liStuntAttack, s32 liMarkedMan);
    void SetProgressionRankEventWins(s32 liOfflineRaceRankWins, s32 liRoadRageRankWins,
                                     s32 liStuntAttackRankWins, s32 liMarkedManRankWins);
};
}
}
