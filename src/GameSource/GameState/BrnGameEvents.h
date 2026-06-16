#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerTeam
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"  // BrnNetwork::NetworkPlayerID

// Owning header for the BrnGameState::GameStateModuleIO GameEvent<> family slices reconstructed
// by the GameMode/ModeManager leaf batch. Minimal slices: only members the reconstructed bodies
// touch. GameEvent<T> is an empty template spine (the real build adds only a static type tag);
// its real Event base + the per-event mseType definitions land with the full BrnGameEvents TU.

namespace BrnGameState
{
namespace GameStateModuleIO
{
// Max players in a network game (== BrnWorld::KI_MAX_ACTIVE_RACE_CARS on this build).
const s32 KI_MAX_RACE_CARS = 8;

// EGameEventType discriminant. Only the slots this batch instantiates are listed; the unconfirmed
// values are placeholders used purely as template tags.
enum EGameEventType
{
    E_EVENT_CHANGE_NETWORK_CAR      = 7,
    E_EVENT_ONLINE_PLAYER_ADDED     = 127,
    E_EVENT_ONLINE_PLAYER_FINALISED = 128,
    E_EVENT_ONLINE_PLAYER_REMOVED   = 129,   // value unconfirmed (template tag only)
    E_EVENT_START_NETWORK_GAME      = 130,   // value unconfirmed (template tag only)
    E_EVENT_REMOTE_PLAYER_DISCONNECTED = 131, // value unconfirmed (template tag only)
    E_EVENT_RECORD_PROP_HIT         = 111,   // DWARF BrnGameEvents.h:121
    E_EVENT_OVERHEAD_SIGN_HIT       = 118,   // DWARF BrnGameEvents.h:76
};

template <EGameEventType T>
struct GameEvent { };

// X360 element of EventQueue<HitOverheadSignEvent,100> (DWARF BrnGameEvents.h:429). Single byte.
struct HitOverheadSignEvent : public GameEvent<E_EVENT_OVERHEAD_SIGN_HIT>
{
    u8 muRaceCarId;   // 0x00
};

// X360 element of EventQueue<RecordPropHitEvent,50> (DWARF BrnGameEvents.h:413). 16-byte aligned
// via the leading Vector3.
struct RecordPropHitEvent : public GameEvent<E_EVENT_RECORD_PROP_HIT>
{
    Vector3 mPosition;   // 0x00 (rw::math::vpu, 16-byte SIMD)
    u16     muZoneId;    // 0x10
    u16     muPropId;    // 0x12
    bool    mbHitBefore; // 0x14
};

// X360 0x823A78F0. mNetworkPlayerID at offset 0.
struct ChangeNetworkCarEvent : public GameEvent<E_EVENT_CHANGE_NETWORK_CAR>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A77D0. mNetworkPlayerID at offset 0x10 (after two CgsID).
struct OnlinePlayerAddedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_ADDED>
{
    CgsID                       mModelID;          // 0x00
    CgsID                       mWheelID;          // 0x08
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;  // 0x10

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7830. mNetworkPlayerID at offset 0.
struct OnlinePlayerFinalisedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_FINALISED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7890. mNetworkPlayerID at offset 0.
struct OnlinePlayerRemovedEvent : public GameEvent<E_EVENT_ONLINE_PLAYER_REMOVED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x823A7770. mNetworkPlayerID at offset 0.
struct RemotePlayerDisconnectedEvent : public GameEvent<E_EVENT_REMOTE_PLAYER_DISCONNECTED>
{
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;

    void SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID);
};

// X360 0x82542068 (Clear) / 0x825420C8 (SetPlayerData). Minimal slice: only the members those
// two functions touch (the per-player arrays + the cleared scalars).
struct StartNetworkGameEvent : public GameEvent<E_EVENT_START_NETWORK_GAME>
{
    bool                        mbRefreshOnly;
    CgsID                       maCarIds[KI_MAX_RACE_CARS];
    u16                         mau16CarColourIndex[KI_MAX_RACE_CARS];
    u16                         mau16CarPaintFinishIndex[KI_MAX_RACE_CARS];
    EPlayerTeam                 maePlayerTeam[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID maNetworkPlayerID[KI_MAX_RACE_CARS];
    f32                         mafPlayerData[KI_MAX_RACE_CARS];
    bool                        mabPlayerHasFever[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID mLocalNetworkPlayerID;
    s32                         miHostGridPosition;
    bool                        mbIsStartingGameAfterPlayerJoin;
    bool                        mbIsStartingFreeburnLobbyAfterOnlineEvent;

    void Clear();
    void SetPlayerData(s32                         liPlayerIndex,
                       BrnNetwork::NetworkPlayerID liNetworkPlayerID,
                       CgsID                       lCarId,
                       f32                         lfPlayerData,
                       u16                         lu16CarColourIndex,
                       u16                         lu16CarPaintFinishIndex,
                       EPlayerTeam                 lePlayerTeam,
                       bool                        lbPlayerHasFever,
                       bool                        lbUpdateLocalNetworkPlayerID);
};
}
}
