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
}
}
