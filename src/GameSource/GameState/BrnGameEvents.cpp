#include "GameSource/GameState/BrnGameEvents.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameEvents.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"   // CgsNetwork::K_INVALID_PLAYER_ID

namespace BrnGameState
{
namespace GameStateModuleIO
{
// X360 0x823A78F0.
void ChangeNetworkCarEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 3362 /* ARTIST 0x823A791C */));
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A77D0.
void OnlinePlayerAddedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 3207 /* ARTIST 0x823A77FC */));
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7830.
void OnlinePlayerFinalisedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 3221 /* ARTIST 0x823A785C */));
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7890.
void OnlinePlayerRemovedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 3331 /* ARTIST 0x823A78BC */));
    mNetworkPlayerID = lNetworkPlayerID;
}

// X360 0x823A7770. Store the disconnecting player's network id (must not be the invalid sentinel).
void RemotePlayerDisconnectedEvent::SetNetworkPlayerID(BrnNetwork::NetworkPlayerID lNetworkPlayerID)
{
    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 3189 /* ARTIST 0x823A779C */));
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

// Populate one player's slot; the host's slot index becomes the host grid position.
void StartNetworkGameEvent::SetPlayerData(s32                         liPlayerIndex,
                                          BrnNetwork::NetworkPlayerID lNetworkPlayerID,
                                          CgsID                       lCarId,
                                          CgsID                       lWheelId,
                                          u16                         lu16CarColourIndex,
                                          u16                         lu16CarPaintFinishIndex,
                                          f32                         lfPlayerData,
                                          EPlayerTeam                 lePlayerTeam,
                                          bool                        lbIsHost,
                                          bool                        lbPlayerHasFever)
{
    (void)lWheelId;

    CGS_ASSERT(lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
        CGS_ASSERT_MESSAGE_AT(
            "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 3447 /* ARTIST 0x82542104 */));

    maNetworkPlayerID[liPlayerIndex]        = lNetworkPlayerID;
    maCarIds[liPlayerIndex]                 = lCarId;
    mau16CarColourIndex[liPlayerIndex]      = lu16CarColourIndex;
    mau16CarPaintFinishIndex[liPlayerIndex] = lu16CarPaintFinishIndex;
    mafPlayerData[liPlayerIndex]            = lfPlayerData;
    maePlayerTeam[liPlayerIndex]            = lePlayerTeam;
    mabPlayerHasFever[liPlayerIndex]        = lbPlayerHasFever;

    if (lbIsHost)
    {
        miHostGridPosition = liPlayerIndex;
    }
}
}
}

#undef CGS_ARTIST_FILE
