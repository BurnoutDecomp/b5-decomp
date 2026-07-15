#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // EPlayerTeam
#include "GameSource/GameState/BrnGameStateTypes.h"          // BrnGameState::LandmarkIndex (StartNetworkRoundEvent)
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
    E_EVENT_START_NETWORK_GAME      = 17,    // DWARF BrnGameEvents.h (full contiguous EGameEventType)
    E_EVENT_START_NETWORK_ROUND     = 18,    // DWARF BrnGameEvents.h
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

// FLAG (minimal home): FinishedSyncingPlayersEvent -- a BrnGameState::GameStateModuleIO network-sync
// notification event queued by BrnNetwork::StateManager::UpdateSyncTime via
// CgsModule::VariableEventQueue<1536,16>::AddEvent<FinishedSyncingPlayersEvent> @ 0x82566168, which
// forwards liSize == sizeof(EventT) == 1 (li r6,1 @ 0x82566204). Neither the member layout nor the
// EGameEventType discriminant is attested by any decompiled caller or by DWARF (the sole caller
// UpdateSyncTime is not yet decompiled), so this is homed at the asm-attested size only: a 1-byte
// opaque marker. Grow to the real GameEvent<E_EVENT_...> shape once the caller is recovered.
struct FinishedSyncingPlayersEvent
{
    u8 muPad0;   // 0x00 -- asm-attested size (1) only; real field(s)/discriminant unknown
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

// X360 0x82542068 (Clear) / 0x825420C8 (SetPlayerData). Full 256-byte X360 layout (grown from the
// former minimal slice): the three X360 facts that NetworkRoundManager::NetworkGameStarted bakes --
// miNumRounds @ offset 8, mbIsStartingGameAfterPlayerJoin @ offset 248, sizeof == 256 (memcpy) --
// all land EXACTLY with this member set/order. mafPlayerData[8] is RETAINED (it pushes
// mbIsStartingGameAfterPlayerJoin to 248; absent from the PS3 DWARF, but the X360 offset proves it
// real). meBoostType spelled s32 (BrnNetwork::EBoostType has no committed home; same 4B storage).
struct StartNetworkGameEvent : public GameEvent<E_EVENT_START_NETWORK_GAME>
{
    s32                         miNumRaceCars;                            // 0x00
    EGameModeType               meGameMode;                               // 0x04
    s32                         miNumRounds;                              // 0x08
    u32                         muRandomSeedForGame;                      // 0x0C
    bool                        mbRefreshOnly;                            // 0x10
    CgsID                       maCarIds[KI_MAX_RACE_CARS];               // 0x18
    u16                         mau16CarColourIndex[KI_MAX_RACE_CARS];
    u16                         mau16CarPaintFinishIndex[KI_MAX_RACE_CARS];
    EPlayerTeam                 maePlayerTeam[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID maNetworkPlayerID[KI_MAX_RACE_CARS];
    f32                         mafPlayerData[KI_MAX_RACE_CARS];          // 0xB8
    bool                        mabPlayerHasFever[KI_MAX_RACE_CARS];
    BrnNetwork::NetworkPlayerID mLocalNetworkPlayerID;                    // 0xE0 (224)
    s32                         miHostGridPosition;                       // 0xE4 (228)
    s32                         miNumRunnerCrashes;                       // 0xE8 (232)
    s32                         meBoostType;                              // 0xEC (236) BrnNetwork::EBoostType (s32)
    f32                         mfTimeLimit;                              // 0xF0 (240)
    bool                        mbRedTeamHaveInfiniteBoost;               // 0xF4 (244)
    bool                        mbIsTrafficOn;                            // 0xF5 (245)
    bool                        mbIsTrafficCheckingOn;                    // 0xF6 (246)
    bool                        mbIsRanked;                               // 0xF7 (247)
    bool                        mbIsStartingGameAfterPlayerJoin;          // 0xF8 (248)
    bool                        mbIsStartingFreeburnLobbyAfterOnlineEvent; // 0xF9 (249)
    bool                        mbForceStartFreeburnLobby;                // 0xFA (250)

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

// X360 element copied as 10 dwords (40 bytes) by NetworkRoundManager::NetworkRoundStarted
// (0x823589E8). sizeof == 40: 16*LandmarkIndex(2) + u32 + s32. mLightTriggerID spelled u32
// (LightTriggerId == u32; matches the BrnGameStateSharedIO.h precedent).
struct StartNetworkRoundEvent : public GameEvent<E_EVENT_START_NETWORK_ROUND>
{
    BrnGameState::LandmarkIndex maLandmarks[16];      // 0x00, 16*2 = 32 bytes
    u32                         mLightTriggerID;      // 0x20 (LightTriggerId == u32)
    s32                         miNumLandmarksInRound; // 0x24
}; // sizeof == 40
}
}
