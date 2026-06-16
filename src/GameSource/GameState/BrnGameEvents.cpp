#include "GameSource/GameState/BrnGameEvents.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::K_INVALID_PLAYER_ID has no committed home (it lives in the un-reconstructed
// CgsNetworkConstants.h); modelled file-local as -1, exactly as the committed
// BrnGameStateFlybyManager.cpp does. The assert expression strings keep the original spelling.
namespace CgsNetwork
{
static const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x823A78F0.
void ChangeNetworkCarEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    if (lNetworkPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameEvents.h",
            3362);
        CgsDev::Assert::EndAssert();
    }
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A77D0.
void OnlinePlayerAddedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    if (lNetworkPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameEvents.h",
            3207);
        CgsDev::Assert::EndAssert();
    }
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7830.
void OnlinePlayerFinalisedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7890.
void OnlinePlayerRemovedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    if (lNetworkPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameEvents.h",
            3331);
        CgsDev::Assert::EndAssert();
    }
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7770. Store the disconnecting player's network id (must not be the invalid sentinel).
void RemotePlayerDisconnectedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    if (lNetworkPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameEvents.h",
            3189);
        CgsDev::Assert::EndAssert();
    }
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x82542068. Reset the start-network-game event to empty defaults: clear the five
// per-player arrays the X360 loop touches, then the four standalone scalars. mabPlayerHasFever
// is deliberately NOT reset (no cursor touches it), preserved faithfully.
void StartNetworkGameEvent::Clear()
{
    for (s32 li = 0; li < KI_MAX_RACE_CARS; ++li)
    {
        maCarIds[li]                 = 0;
        mau16CarColourIndex[li]      = 0;
        mau16CarPaintFinishIndex[li] = 0;
        maePlayerTeam[li]            = E_PLAYER_TEAM_NONE;
        maNetworkPlayerID[li]        = 0;
    }

    mbRefreshOnly                             = false;
    miHostGridPosition                        = 0;
    mbIsStartingGameAfterPlayerJoin           = false;
    mbIsStartingFreeburnLobbyAfterOnlineEvent = false;
}

// X360 0x825420C8. Populate one player's slot, optionally adopting it as the local player.
void StartNetworkGameEvent::SetPlayerData(s32                         liPlayerIndex,
                                          BrnNetwork::NetworkPlayerID liNetworkPlayerID,
                                          CgsID                       lCarId,
                                          f32                         lfPlayerData,
                                          u16                         lu16CarColourIndex,
                                          u16                         lu16CarPaintFinishIndex,
                                          EPlayerTeam                 lePlayerTeam,
                                          bool                        lbPlayerHasFever,
                                          bool                        lbUpdateLocalNetworkPlayerID)
{
    CGS_ASSERT(liNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
               "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    maNetworkPlayerID[liPlayerIndex]        = liNetworkPlayerID;
    maCarIds[liPlayerIndex]                 = lCarId;
    mau16CarColourIndex[liPlayerIndex]      = lu16CarColourIndex;
    mau16CarPaintFinishIndex[liPlayerIndex] = lu16CarPaintFinishIndex;
    mafPlayerData[liPlayerIndex]            = lfPlayerData;
    maePlayerTeam[liPlayerIndex]            = lePlayerTeam;
    mabPlayerHasFever[liPlayerIndex]        = lbPlayerHasFever;

    if (lbUpdateLocalNetworkPlayerID)
    {
        mLocalNetworkPlayerID = liPlayerIndex;
    }
}
}
}
