#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsPlayerManager.cpp"
namespace CgsNetwork
{
// The console ctor only runs the embedded members' construction: the connection manager,
// the ack/nack signal messages and the debug component.
PlayerManager::PlayerManager()
{
}
}

// FOLDED FROM CgsPlayerManager_wN2_01.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // NetworkPlayer virtuals / Destruct
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"          // NetworkAdapter::mpMessageSentCallbackFunction
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"            // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include "types.hpp"   // intptr_t
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// CgsNetwork::PlayerManager -- the registry lifecycle (Construct / Prepare / Release /
// Destruct), the per-frame Update / PostUpdate / SendMessages pumps, the round edges, the
// lobby / disconnect edges and the game-id stamp.

namespace CgsNetwork
{

// The GetNextPlayerID perfmon handle starts out invalid; Prepare registers it once.
s32  PlayerManager::miNextPlayerIDPerfmon = -1;
bool PlayerManager::_mbRegisteredPerfmon  = false;

// The message-sent hook Prepare installs on the network adapter. Empty in this build.
static void MessageSentCallbackFunction(void* lpUserData)
{
    (void)lpUserData;
}

// ---- Construct -----------------------------------------------------------------------
// Point every network player's construct params at the registry and construct the players,
// seed the inactive table from the caller's tables (the last slot has menu data only), then
// reset every member, the ack/nack messages, the two embedded managers, the debug component
// and the bandwidth counters.
void PlayerManager::Construct(PlayerManagerConstructParams* lpConstructParams)
{
    CGS_ASSERT(lpConstructParams->mapPlayerList, CGS_ASSERT_MESSAGE_AT(
        "lpConstructParams->mapPlayerList",
        CGS_ARTIST_FILE, 77 /* ARTIST 0x82895444 */));

    // Every slot but the last carries a network player.
    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS - 1; ++liIndex)
    {
        CGS_ASSERT(lpConstructParams->mapConstructParams[liIndex], CGS_ASSERT_MESSAGE_AT(
            "lpConstructParams->mapConstructParams[liIndex]",
            CGS_ARTIST_FILE, 82 /* ARTIST 0x82895478 */));
        lpConstructParams->mapConstructParams[liIndex]->mpPlayerManager = this;
        lpConstructParams->mapPlayerList[liIndex]->Construct(lpConstructParams->mapConstructParams[liIndex]);
    }

    s32 liIndex = 0;
    for (; liIndex < KI_MAX_PLAYERS - 1; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(lpConstructParams->mapPlayerList[liIndex],
                                       lpConstructParams->mapMenuData[liIndex]);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(0, 0);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }
    for (; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(0, lpConstructParams->mapMenuData[liIndex]);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(0, 0);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers               = 0;
    miNumInactivePlayers             = KI_MAX_PLAYERS;
    mpNetworkAdapter                 = 0;
    mu16CurrentFrame                 = 0;
    mpfOnReceivedFromWrongIPCallback = 0;
    mpfConnectionFinalisedCallback   = 0;
    mpConnectionFinalisedUserData    = 0;
    mHostPlayerID                    = K_INVALID_PLAYER_ID;
    meLocalConsoleFrameRate          = CgsSystem::E_FRAMERATE_UNKNOWN;
    mLocalPlayerID                   = K_INVALID_PLAYER_ID;
    mu8GameID                        = KU8_INVALID_GAME_ID;

    for (s32 liAck = 0; liAck < KI_MAX_ACKS_TO_BUFFER; ++liAck)
    {
        maAckMessage[liAck].ResetMessageAndPlayerIDs();
    }
    for (s32 liNack = 0; liNack < KI_MAX_NACKS_TO_BUFFER; ++liNack)
    {
        maNackMessage[liNack].ResetMessageAndPlayerIDs();
    }

    mReliableMessageManager.Construct();
    mConnectionManager.Construct();

    mpServerInterface = 0;
    mePrepareState    = E_CONSTRUCTED;
    ResetEventCallbacks();
    mbDiskAccessible  = true;

    mDebugComponent.Construct(this);
    ResetAllBandwidthCounters();
}

// ---- Destruct ------------------------------------------------------------------------
// Destruct every player object still referenced by either table, empty both tables and
// reset the registry to its released state.
void PlayerManager::Destruct()
{
    ResetEventCallbacks();

    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        if (maActivePlayers[liIndex].GetNetworkPlayer())
        {
            maActivePlayers[liIndex].GetNetworkPlayer()->Destruct();
        }
    }
    for (s32 liIndex = 0; liIndex < miNumInactivePlayers; ++liIndex)
    {
        if (maInactivePlayers[liIndex].GetNetworkPlayer())
        {
            maInactivePlayers[liIndex].GetNetworkPlayer()->Destruct();
        }
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_PLAYERS; ++liIndex)
    {
        maInactivePlayers[liIndex].Set(0, 0);
        maInactivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
        maActivePlayers[liIndex].Set(0, 0);
        maActivePlayers[liIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers      = 0;
    miNumInactivePlayers    = 0;
    mpNetworkAdapter        = 0;
    mpServerInterface       = 0;
    meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
    mpTimeManager           = 0;
    mLocalPlayerID          = K_INVALID_PLAYER_ID;
    mbDiskAccessible        = true;

    mReliableMessageManager.Destruct();
    mePrepareState = E_RELEASED;

    mDebugComponent.Destruct();
    ResetAllBandwidthCounters();
}

// ---- Prepare -------------------------------------------------------------------------
// A resumable state machine: bind the session objects, then prepare the connection
// manager, then the reliable-message queue. A step that fails returns false and leaves the
// state on that step so the next call retries it. The two registry perfmons are registered
// on the first call.
bool PlayerManager::Prepare(PlayerManagerPrepareParams* lpPrepareParams)
{
    CGS_ASSERT_AT(miNumActivePlayers == 0, "miNumActivePlayers == 0", CGS_ARTIST_FILE, 225);

    switch (mePrepareState)
    {
    case E_CONSTRUCTED:
    case E_RELEASED:
        CGS_ASSERT_AT(miNumActivePlayers == 0, "miNumActivePlayers == 0", CGS_ARTIST_FILE, 232);
        CGS_ASSERT_AT(miNumInactivePlayers == KI_MAX_PLAYERS,
                      "miNumInactivePlayers == KI_MAX_PLAYERS", CGS_ARTIST_FILE, 233);

        mpNetworkAdapter                 = lpPrepareParams->mpNetworkAdapter;
        mu8GameID                        = KU8_INVALID_GAME_ID;
        mpfOnReceivedFromWrongIPCallback = lpPrepareParams->mpfOnReceivedFromWrongIPCallback;
        mpServerInterface                = lpPrepareParams->mpServerInterface;
        meLocalConsoleFrameRate          = lpPrepareParams->meLocalConsoleFrameRate;
        mLocalPlayerID                   = K_INVALID_PLAYER_ID;
        mpTimeManager                    = lpPrepareParams->mpTimeManager;

        CGS_ASSERT_AT(meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
                          meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
                      "meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ",
                      CGS_ARTIST_FILE, 247);

        mpNetworkAdapter->mpMessageSentCallbackFunction =
            reinterpret_cast<void*>(&MessageSentCallbackFunction);

        mePrepareState = E_PREPARING_CONNECTION_MANAGER;
        mDebugComponent.Prepare();
        ResetAllBandwidthCounters();
        // fall through

    case E_PREPARING_CONNECTION_MANAGER:
        mePrepareState                 = E_PREPARING_CONNECTION_MANAGER;
        mpfConnectionFinalisedCallback = lpPrepareParams->mpfConnectionFinalisedCallback;
        mpConnectionFinalisedUserData  = lpPrepareParams->mpConnectionFinalisedUserData;
        if (!mConnectionManager.Prepare(this, lpPrepareParams->mpServerInterface,
                                        ConnectionFinalisedCallback, this,
                                        PlayerDisconnectedCallback, this))
        {
            return false;
        }
        mePrepareState = E_PREPARING_RELIABLE_MESSAGE_MANAGER;
        // fall through

    case E_PREPARING_RELIABLE_MESSAGE_MANAGER:
        mePrepareState = E_PREPARING_RELIABLE_MESSAGE_MANAGER;
        if (!mReliableMessageManager.Prepare(this, lpPrepareParams->mpNetworkHeapAllocator))
        {
            return false;
        }
        mePrepareState = E_FULLY_PREPARED;
        // fall through

    case E_FULLY_PREPARED:
        mePrepareState = E_FULLY_PREPARED;
        ResetEventCallbacks();
        break;

    default:
        CGS_ASSERT_STREAM_AT(false, "PlayerManager: unknown prepare state " << static_cast<s32>(mePrepareState) << "\n",
                             CGS_ARTIST_FILE, 304);
        break;
    }

    if (!_mbRegisteredPerfmon)
    {
        miNextPlayerIDPerfmon = CgsDev::PerfMonCpu::AddMonitor("PLAYERManager - GetNextPlayerID",
                                                               CgsDev::E_PMP_9, false, 5.0f, true);
        miPLAYERManagerSendMessagesPM = CgsDev::PerfMonCpu::AddMonitor("PLAYERManager - SendMessages",
                                                                       CgsDev::E_PMP_9, false, 5.0f, true);
        _mbRegisteredPerfmon = true;
    }

    return true;
}

// ---- Release -------------------------------------------------------------------------
// Release every active player, forget the session objects, release both embedded managers
// and drop back to E_RELEASED. Fails (state unchanged) when the reliable-message queue does.
bool PlayerManager::Release()
{
    while (miNumActivePlayers > 0)
    {
        ReleasePlayer(0);
    }

    CGS_ASSERT(miNumActivePlayers == 0, CGS_ASSERT_MESSAGE_AT(
        "miNumActivePlayers == 0",
        CGS_ARTIST_FILE, 342 /* ARTIST 0x82895B5C */));
    CGS_ASSERT(miNumInactivePlayers == KI_MAX_PLAYERS, CGS_ASSERT_MESSAGE_AT(
        "miNumInactivePlayers == KI_MAX_PLAYERS",
        CGS_ARTIST_FILE, 343 /* ARTIST 0x82895B84 */));

    meLocalConsoleFrameRate = CgsSystem::E_FRAMERATE_UNKNOWN;
    mLocalPlayerID          = K_INVALID_PLAYER_ID;
    mpServerInterface       = 0;
    mpNetworkAdapter        = 0;
    mpTimeManager           = 0;
    ResetEventCallbacks();

    mDebugComponent.Release();
    ResetAllBandwidthCounters();

    mConnectionManager.Release();
    if (!mReliableMessageManager.Release())
    {
        return false;
    }

    mePrepareState = E_RELEASED;
    return true;
}

// ---- Update --------------------------------------------------------------------------
// Latch the frame; once fully prepared, drain the receive side, advance the connection
// tests and the reliable queue, then update every network player and broadcast the
// lost / regained contact edge when a player's contact flag flips during its update.
void PlayerManager::Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame,
                           bool lbInGame)
{
    mu16CurrentFrame = lu16CurrentFrame;
    if (mePrepareState != E_FULLY_PREPARED)
    {
        return;
    }

    ReceiveMessages();
    mConnectionManager.Update(lpTimerStatus, mu16CurrentFrame);
    mReliableMessageManager.Update();

    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer == 0)
        {
            continue;
        }

        const bool lbTalking = !lpNetworkPlayer->mbNetworkPlayerPaused;
        lpNetworkPlayer->Update(lpTimerStatus, mu16CurrentFrame, lbInGame);
        if (lbTalking != !lpNetworkPlayer->mbNetworkPlayerPaused)
        {
            if (!lpNetworkPlayer->mbNetworkPlayerPaused)
                BroadcastEvent(E_EVENT_PLAYER_REGAINED_CONTACT, reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
            else
                BroadcastEvent(E_EVENT_PLAYER_LOST_CONTACT, reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
        }
    }
}

// ---- PostUpdate ----------------------------------------------------------------------
// Once fully prepared: the frame must match the one Update latched; then run the send pump.
void PlayerManager::PostUpdate(u16 lu16CurrentFrame)
{
    if (mePrepareState != E_FULLY_PREPARED)
    {
        return;
    }

    CGS_ASSERT(mu16CurrentFrame == lu16CurrentFrame,
        CGS_ASSERT_MESSAGE_AT(
            "mu16CurrentFrame == lu16CurrentFrame",
            CGS_ARTIST_FILE, 451 /* ARTIST 0x8289B8F0 */));
    mu16CurrentFrame = lu16CurrentFrame;
    SendMessages();
}

// ---- SendMessages --------------------------------------------------------------------
// Pump every active network player's sends; while the disk is unreadable the players'
// queued messages are dropped instead.
void PlayerManager::SendMessages()
{
    CgsDev::PerfMonCpu::StartMonitor(miPLAYERManagerSendMessagesPM);

    // DWARF (CgsPlayerManager.cpp:730) holds only `i`: the accessor is tested and called directly
    // (ARTIST cmpwi at 0x8289A848).
    for (s32 i = 0; i < miNumActivePlayers; ++i)
    {
        if (maActivePlayers[i].GetNetworkPlayer())
        {
            if (mbDiskAccessible)
            {
                maActivePlayers[i].GetNetworkPlayer()->SendMessages();
            }
            else
            {
                maActivePlayers[i].GetNetworkPlayer()->ResetAllMessages();
            }
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miPLAYERManagerSendMessagesPM);
}

// ---- OnRoundStart --------------------------------------------------------------------
// Forget the received-message window and tell every finalised network player.
void PlayerManager::OnRoundStart()
{
    mReliableMessageManager.ClearRcvdReliableMessages();

    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        if (lpNetworkPlayer != 0)
        {
            lpNetworkPlayer->OnRoundStart();
        }
    }
}

// ---- OnRoundFinish -------------------------------------------------------------------
// Drop every buffered outgoing reliable message addressed to a finalised player.
void PlayerManager::OnRoundFinish()
{
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerID, E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
    {
        mReliableMessageManager.ClearPlayersSendReliableMessages(lPlayerID);
    }
}

// ---- OnLobbyApiCreated ---------------------------------------------------------------
void PlayerManager::OnLobbyApiCreated()
{
    mConnectionManager.OnLobbyApiCreated();
}

// ---- Disconnected --------------------------------------------------------------------
// The session is gone: move every active player back to the inactive table (releasing its
// network player), forget the host, the local player and the game id, and reset the
// connection-test entries.
void PlayerManager::Disconnected()
{
    mHostPlayerID = K_INVALID_PLAYER_ID;

    for (s32 liPlayerIndex = 0; liPlayerIndex < miNumActivePlayers; ++liPlayerIndex)
    {
        maInactivePlayers[miNumInactivePlayers] = maActivePlayers[liPlayerIndex];
        ++miNumInactivePlayers;

        // DWARF: liPlayerIndex is the only local; the accessor is tested directly (ARTIST cmpwi 0x828847F4).
        if (maActivePlayers[liPlayerIndex].GetNetworkPlayer())
        {
            maActivePlayers[liPlayerIndex].GetNetworkPlayer()->Release();
        }
        maActivePlayers[liPlayerIndex].SetNetworkPlayer(0);
        maActivePlayers[liPlayerIndex].SetPlayerID(K_INVALID_PLAYER_ID);
    }

    miNumActivePlayers = 0;
    mLocalPlayerID     = K_INVALID_PLAYER_ID;
    mu8GameID          = KU8_INVALID_GAME_ID;
    mConnectionManager.Disconnected();
}

// ---- SetGameID -----------------------------------------------------------------------
// Adopt a new session game id (the invalid id is stored as 0) and reset every ack / nack
// signal message. The messages are stamped with game id 0, not the new id. In the nack
// pass the console tests each nack slot's valid flag but clears the matching ack slot's.
void PlayerManager::SetGameID(s32 liGameID)
{
    const u8 lu8GameID = static_cast<u8>(liGameID);
    if (mu8GameID == lu8GameID)
    {
        return;
    }

    mu8GameID = lu8GameID;
    if (lu8GameID == KU8_INVALID_GAME_ID)
    {
        mu8GameID = 0;
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_ACKS_TO_BUFFER; ++liIndex)
    {
        if (maAckMessage[liIndex].IsMessageValid())
        {
            maAckMessage[liIndex].SetMessageInvalid();
        }
        maAckMessage[liIndex].ResetMessageAndPlayerIDs();
        maAckMessage[liIndex].SetGameID(0);
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_NACKS_TO_BUFFER; ++liIndex)
    {
        if (maNackMessage[liIndex].IsMessageValid())
        {
            maAckMessage[liIndex].SetMessageInvalid();
        }
        maNackMessage[liIndex].ResetMessageAndPlayerIDs();
        maNackMessage[liIndex].SetGameID(0);
    }
}

}

// FOLDED FROM CgsPlayerManager_wN2_02.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // NetworkPlayer / PlayerMenuData
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"      // SignalMessage::PrepareAck / PrepareNack
#include "lobbyname.h"                                                                  // LobbyNameCmp
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                       // TimeManager::GetFrameCount
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"        // GetConnectionComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"                 // GetLobbyAPIRef
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"     // ServerInterfaceConnection
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"            // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "lobbyapi.h"                                                                  // LobbyApiDisconnect

#include "types.hpp"   // intptr_t
#include <cstring>   // strlen / strncpy

#include "netconn.h"   // NetConnStatus

// CgsNetwork::PlayerManager -- the player registry proper: adding and removing players, the
// active / inactive table moves, the id walks and lookups, host identity, the event
// callbacks, the connection-manager callbacks, the ack / nack filters for received
// messages and the round-robin send slot.

// Preserve the ARTIST location for assertions recovered from this translation unit.

namespace CgsNetwork
{

// The 'plug' NetConnStatus selector: zero when the network cable is out.
static const s32 KI_NETCONN_STATUS_PLUG = 0x706C7567;

// ---- AddPlayer -----------------------------------------------------------------------
// Move a free inactive slot into maActivePlayers[liConnectionIndex] (a local slot has no
// network player; a remote one prepares its network player), reset the slot's menu data
// for this player, take the host id from connection index 0 and tell the listeners.
void PlayerManager::AddPlayer(const CgsSystem::TimerStatus* lpTimerStatus, const char* lpcName,
                              NetworkPlayerID lPlayerID, s32 liConnectionIndex,
                              CgsSystem::EFrameRate leRemoteConsoleFrameRate, bool lbLocal)
{
    CGS_ASSERT(liConnectionIndex < KI_MAX_PLAYERS, "liConnectionIndex < KI_MAX_PLAYERS");
    CGS_ASSERT(liConnectionIndex >= 0, "liConnectionIndex >= 0");
    CGS_ASSERT(maActivePlayers[liConnectionIndex].GetNetworkPlayer() == NULL,
               "maActivePlayers[liConnectionIndex].GetNetworkPlayer() == NULL");
    CGS_ASSERT(miNumInactivePlayers > 0, "miNumInactivePlayers > 0");
    CGS_ASSERT(meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ ||
                   meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ,
               "meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_50HZ || meLocalConsoleFrameRate == CgsSystem::E_FRAMERATE_60HZ");
    CGS_ASSERT(mpServerInterface, "mpServerInterface");

    PlayerData* lpPlayerData;
    if (lbLocal)
    {
        lpPlayerData = AssignActiveLocalPlayer(liConnectionIndex);
        lpPlayerData->SetPlayerID(lPlayerID);
        CGS_ASSERT(mLocalPlayerID == K_INVALID_PLAYER_ID,
                   "More than one local player with D_ONLY_ONE_LOCAL_PLAYER defined\n");
        mLocalPlayerID = lPlayerID;
    }
    else
    {
        lpPlayerData = AssignActiveNetworkPlayer(liConnectionIndex);
        lpPlayerData->SetPlayerID(lPlayerID);
        lpPlayerData->GetNetworkPlayer()->Prepare(mpNetworkAdapter, lpTimerStatus, lpcName,
                                                  meLocalConsoleFrameRate,
                                                  leRemoteConsoleFrameRate, lPlayerID);
    }

    lpPlayerData->GetMenuData()->Clear();
    lpPlayerData->GetMenuData()->miConnectionIndex = liConnectionIndex;

    // The bounded name copy: the console streams the name after the literal.
    PlayerMenuData* lpMenuData = lpPlayerData->GetMenuData();
    CGS_ASSERT(strlen(lpcName) < sizeof(lpMenuData->macName), "String too long: ");
    strncpy(lpMenuData->macName, lpcName, sizeof(lpMenuData->macName));

    if (liConnectionIndex == 0)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "CgsNetwork::PlayerManager::AddPlayer"
                                   << ": setting hostID to " << lPlayerID << "\n";
        mHostPlayerID = lPlayerID;
    }

    BroadcastEvent(E_EVENT_PLAYER_ADDED, reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
}

// ---- RemovePlayer --------------------------------------------------------------------
// Bracket the release of an active player with the start / end removal events and forget
// it as host or local player.
void PlayerManager::RemovePlayer(NetworkPlayerID lPlayerID)
{
    BroadcastEvent(E_EVENT_START_PLAYER_REMOVAL,
                   reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));

    s32 liPlayerIndex = 0;
    for (; liPlayerIndex < miNumActivePlayers; ++liPlayerIndex)
    {
        if (maActivePlayers[liPlayerIndex].GetPlayerID() == lPlayerID)
        {
            break;
        }
    }
    CGS_ASSERT(liPlayerIndex < miNumActivePlayers,
        CGS_ASSERT_MESSAGE_AT(
            "liPlayerIndex < miNumActivePlayers", CGS_ARTIST_FILE, 1336 /* ARTIST 0x8288457C */));

    ReleasePlayer(liPlayerIndex);

    if (mHostPlayerID == lPlayerID)
    {
        mHostPlayerID = K_INVALID_PLAYER_ID;
    }
    if (lPlayerID == mLocalPlayerID)
    {
        mLocalPlayerID = K_INVALID_PLAYER_ID;
    }

    BroadcastEvent(E_EVENT_END_PLAYER_REMOVAL,
                   reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
}

// ---- AreAnyPlayersConnected ----------------------------------------------------------
// True while any active slot holds a network player that has not been flagged
// disconnected. The console folds this into the disconnect callback.
bool PlayerManager::AreAnyPlayersConnected() const
{
    for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
    {
        const NetworkPlayer* lpNetworkPlayer = maActivePlayers[liPlayer].GetNetworkPlayer();
        if (lpNetworkPlayer != 0 && !lpNetworkPlayer->HasConnectionFailed())
        {
            return true;
        }
    }
    return false;
}

// ---- PlayerDisconnectedCallback ------------------------------------------------------
// The connection manager lost a player: tell the listeners, flag the network player
// disconnected, and once nobody is left with the cable out, drop the lobby connection.
void PlayerManager::PlayerDisconnectedCallback(NetworkPlayerID lPlayerID, void* lpUserData)
{
    PlayerManager* lpPlayerManager = static_cast<PlayerManager*>(lpUserData);
    CGS_ASSERT(lpPlayerManager,
        CGS_ASSERT_MESSAGE_AT("lpPlayerManager", CGS_ARTIST_FILE, 1797 /* ARTIST 0x8288460C */));

    lpPlayerManager->BroadcastEvent(E_EVENT_PLAYER_DISCONNECTED,
                                    reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));

    NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
    CGS_ASSERT(lpNetworkPlayer,
        CGS_ASSERT_MESSAGE_AT("lpNetworkPlayer", CGS_ARTIST_FILE, 1803 /* ARTIST 0x8288468C */));
    lpNetworkPlayer->SetDisconnected();

    if (lpPlayerManager->AreAnyPlayersConnected())
    {
        return;
    }

    CGS_ASSERT(lpPlayerManager->mpServerInterface,
        CGS_ASSERT_MESSAGE_AT(
            "lpPlayerManager->mpServerInterface", CGS_ARTIST_FILE, 1809 /* ARTIST 0x828846FC */));
    ServerInterfaceConnection* lpServerInterfaceConnection = static_cast<ServerInterfaceConnection*>(
        lpPlayerManager->mpServerInterface->GetConnectionComponent());
    CGS_ASSERT(lpServerInterfaceConnection,
        CGS_ASSERT_MESSAGE_AT(
            "lpServerInterfaceConnection", CGS_ARTIST_FILE, 1811 /* ARTIST 0x82884728 */));

    if (NetConnStatus(KI_NETCONN_STATUS_PLUG, 0, 0, 0) == 0)
    {
        LobbyApiDisconnect(lpServerInterfaceConnection->GetServerInterface()->GetLobbyAPIRef(), 0);
    }
}

// ---- ConnectionFinalisedCallback -----------------------------------------------------
// Forward the connection manager's verdict to the owner's callback; a success is also a
// player-finalised event.
void PlayerManager::ConnectionFinalisedCallback(bool lbSuccess, NetworkPlayerID lPlayerID,
                                                ConnectionData lConnectionData, void* lpUserData)
{
    PlayerManager* lpPlayerManager = static_cast<PlayerManager*>(lpUserData);

    if (lpPlayerManager->mpfConnectionFinalisedCallback != 0)
    {
        lpPlayerManager->mpfConnectionFinalisedCallback(lbSuccess, lPlayerID, lConnectionData,
                                                        lpPlayerManager->mpConnectionFinalisedUserData);
    }

    if (lbSuccess)
    {
        lpPlayerManager->BroadcastEvent(E_EVENT_PLAYER_FINALISED,
                                        reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));
    }
}

// ---- ReleasePlayer -------------------------------------------------------------------
// Release the active slot's network player, return the slot to the inactive table, close
// the gap in the active table (renumbering each moved player's connection index) and clear
// the freed last slot.
void PlayerManager::ReleasePlayer(s32 liPlayerIndex)
{
    CGS_ASSERT(liPlayerIndex < miNumActivePlayers, CGS_ASSERT_MESSAGE_AT(
        "liPlayerIndex < miNumActivePlayers",
        CGS_ARTIST_FILE, 1370 /* ARTIST 0x828739DC */));

    // No local in DWARF: accessor tested directly (ARTIST cmpwi at 0x828739F8).
    if (maActivePlayers[liPlayerIndex].GetNetworkPlayer())
    {
        maActivePlayers[liPlayerIndex].GetNetworkPlayer()->Release();
    }

    maInactivePlayers[miNumInactivePlayers] = maActivePlayers[liPlayerIndex];
    ++miNumInactivePlayers;

    for (; liPlayerIndex < miNumActivePlayers - 1; ++liPlayerIndex)
    {
        CGS_ASSERT((liPlayerIndex + 1) < KI_MAX_PLAYERS, CGS_ASSERT_MESSAGE_AT(
            "(liPlayerIndex + 1) < KI_MAX_PLAYERS",
            CGS_ARTIST_FILE, 1384 /* ARTIST 0x82873A90 */));
        maActivePlayers[liPlayerIndex] = maActivePlayers[liPlayerIndex + 1];
        maActivePlayers[liPlayerIndex].GetMenuData()->miConnectionIndex = liPlayerIndex;
    }

    maActivePlayers[miNumActivePlayers - 1].SetNetworkPlayer(0);
    maActivePlayers[miNumActivePlayers - 1].SetPlayerID(K_INVALID_PLAYER_ID);
    --miNumActivePlayers;
}

// ---- AssignActiveLocalPlayer ---------------------------------------------------------
// Take the first inactive slot without a network player into the given active slot; the
// inactive table's last entry fills the hole.
PlayerData* PlayerManager::AssignActiveLocalPlayer(s32 liConnectionIndex)
{
    for (s32 liIndex = 0; liIndex < miNumInactivePlayers; ++liIndex)
    {
        if (maInactivePlayers[liIndex].GetNetworkPlayer() == 0)
        {
            maActivePlayers[liConnectionIndex] = maInactivePlayers[liIndex];
            maInactivePlayers[liIndex]         = maInactivePlayers[miNumInactivePlayers - 1];
            --miNumInactivePlayers;
            ++miNumActivePlayers;
            return &maActivePlayers[liConnectionIndex];
        }
    }

    CGS_ASSERT(false,
        CGS_ASSERT_MESSAGE_AT(
            "Couldn't find a free inactive local player", CGS_ARTIST_FILE, 1423 /* ARTIST 0x82873B6C */));
    return 0;
}

// ---- AssignActiveNetworkPlayer -------------------------------------------------------
// As above, for the first inactive slot that carries a network player.
PlayerData* PlayerManager::AssignActiveNetworkPlayer(s32 liConnectionIndex)
{
    for (s32 liIndex = 0; liIndex < miNumInactivePlayers; ++liIndex)
    {
        if (maInactivePlayers[liIndex].GetNetworkPlayer() != 0)
        {
            maActivePlayers[liConnectionIndex] = maInactivePlayers[liIndex];
            maInactivePlayers[liIndex]         = maInactivePlayers[miNumInactivePlayers - 1];
            --miNumInactivePlayers;
            ++miNumActivePlayers;
            return &maActivePlayers[liConnectionIndex];
        }
    }

    CGS_ASSERT(false,
        CGS_ASSERT_MESSAGE_AT(
            "Couldn't find a free inactive network player", CGS_ARTIST_FILE, 1453 /* ARTIST 0x82873C7C */));
    return 0;
}

// ---- GetNextPlayerID -----------------------------------------------------------------
// Step past *lpPlayerID in the active table (from the start when it is the invalid id)
// to the next player: any player, or only local players and remote players whose
// connection has finalised. Exhausted: the invalid id and false.
bool PlayerManager::GetNextPlayerID(NetworkPlayerID* lpPlayerID, EPlayersToConsider leConsider) const
{
    CgsDev::PerfMonCpu::StartMonitor(miNextPlayerIDPerfmon);

    s32 liIndex = 0;
    if (*lpPlayerID != K_INVALID_PLAYER_ID)
    {
        for (; liIndex < miNumActivePlayers; ++liIndex)
        {
            if (maActivePlayers[liIndex].GetPlayerID() == *lpPlayerID)
            {
                break;
            }
        }
        // The console streams the player id between the two literals.
        CGS_ASSERT(liIndex < miNumActivePlayers, "Failed to find player ID  in active players list\n");
        ++liIndex;
    }

    if (leConsider == E_CONSIDER_ALL_PLAYERS)
    {
        if (liIndex < miNumActivePlayers)
        {
            *lpPlayerID = maActivePlayers[liIndex].GetPlayerID();
            CgsDev::PerfMonCpu::StopMonitor(miNextPlayerIDPerfmon);
            return true;
        }
    }
    else
    {
        for (; liIndex < miNumActivePlayers; ++liIndex)
        {
            if (maActivePlayers[liIndex].GetNetworkPlayer() == 0 ||
                mConnectionManager.GetConnectionStatus(maActivePlayers[liIndex].GetPlayerID()) ==
                    E_CONNECTION_SUCCESS)
            {
                *lpPlayerID = maActivePlayers[liIndex].GetPlayerID();
                CgsDev::PerfMonCpu::StopMonitor(miNextPlayerIDPerfmon);
                return true;
            }
        }
    }

    *lpPlayerID = K_INVALID_PLAYER_ID;
    CgsDev::PerfMonCpu::StopMonitor(miNextPlayerIDPerfmon);
    return false;
}

// ---- GetNextRemotePlayerID -----------------------------------------------------------
// The same walk over the active players that have a network player.
bool PlayerManager::GetNextRemotePlayerID(NetworkPlayerID* lpPlayerID) const
{
    s32 liIndex = 0;
    if (*lpPlayerID != K_INVALID_PLAYER_ID)
    {
        for (; liIndex < miNumActivePlayers; ++liIndex)
        {
            if (maActivePlayers[liIndex].GetPlayerID() == *lpPlayerID)
            {
                break;
            }
        }
        // The console streams the player id between the two literals.
        CGS_ASSERT(liIndex < miNumActivePlayers,
                   "Failed to find remote player ID  in active players list\n");
        ++liIndex;
    }

    for (; liIndex < miNumActivePlayers; ++liIndex)
    {
        if (maActivePlayers[liIndex].GetNetworkPlayer() != 0)
        {
            *lpPlayerID = maActivePlayers[liIndex].GetPlayerID();
            return true;
        }
    }

    *lpPlayerID = K_INVALID_PLAYER_ID;
    return false;
}

// ---- GetPlayerByID -------------------------------------------------------------------
// The network player of the active player with this id (null for a local player or an
// unknown id).
NetworkPlayer* PlayerManager::GetPlayerByID(NetworkPlayerID lPlayerID) const
{
    return FindActiveNetworkPlayer(lPlayerID);
}

// ---- GetPlayerByName -----------------------------------------------------------------
// The first active network player whose name matches under the lobby name comparison.
NetworkPlayer* PlayerManager::GetPlayerByName(const char* lpcName)
{
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        NetworkPlayer* lpNetworkPlayer = maActivePlayers[liIndex].GetNetworkPlayer();
        if (lpNetworkPlayer != 0 && ::LobbyNameCmp(lpNetworkPlayer->GetName(), lpcName) == 0)
        {
            return lpNetworkPlayer;
        }
    }
    return 0;
}

// ---- GetMenuDataByID -----------------------------------------------------------------
PlayerMenuData* PlayerManager::GetMenuDataByID(NetworkPlayerID lPlayerID) const
{
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        if (maActivePlayers[liIndex].GetPlayerID() == lPlayerID)
        {
            return maActivePlayers[liIndex].GetMenuData();
        }
    }
    return 0;
}

// ---- host identity -------------------------------------------------------------------
void PlayerManager::SetHostPlayerID(NetworkPlayerID lPlayerID)
{
    mHostPlayerID = lPlayerID;
}

NetworkPlayerID PlayerManager::GetHostPlayerID()
{
    return mHostPlayerID;
}

// Only a session with no players may lack a local player.
bool PlayerManager::AmIHost()
{
    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    GetNextLocalPlayerID(&lLocalPlayerID);
    CGS_ASSERT((lLocalPlayerID != K_INVALID_PLAYER_ID) || (0 == miNumActivePlayers), CGS_ASSERT_MESSAGE_AT(
        "(lLocalPlayerID != K_INVALID_PLAYER_ID) || (0 == miNumActivePlayers)",
        CGS_ARTIST_FILE, 1648 /* ARTIST 0x82873F04 */));
    return mHostPlayerID == lLocalPlayerID;
}

// ---- AckNeedsSending / NackNeedsSending ----------------------------------------------
// Whether ack / nack slot liIndex holds a pending signal from lPlayerID.
bool PlayerManager::AckNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0,
        CGS_ASSERT_MESSAGE_AT("liIndex >= 0", CGS_ARTIST_FILE, 1576 /* ARTIST 0x82873D80 */));
    CGS_ASSERT(liIndex < KI_MAX_ACKS_TO_BUFFER,
        CGS_ASSERT_MESSAGE_AT(
            "liIndex < KI_MAX_ACKS_TO_BUFFER", CGS_ARTIST_FILE, 1577 /* ARTIST 0x82873DA4 */));

    if (!maAckMessage[liIndex].IsMessageValid())
    {
        return false;
    }
    return maAckMessage[liIndex].GetSendingPlayerID() == lPlayerID;
}

bool PlayerManager::NackNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0,
        CGS_ASSERT_MESSAGE_AT("liIndex >= 0", CGS_ARTIST_FILE, 1611 /* ARTIST 0x82873E30 */));
    CGS_ASSERT(liIndex < KI_MAX_NACKS_TO_BUFFER,
        CGS_ASSERT_MESSAGE_AT(
            "liIndex < KI_MAX_NACKS_TO_BUFFER", CGS_ARTIST_FILE, 1612 /* ARTIST 0x82873E54 */));

    if (!maNackMessage[liIndex].IsMessageValid())
    {
        return false;
    }
    return maNackMessage[liIndex].GetSendingPlayerID() == lPlayerID;
}

// ---- event callbacks -----------------------------------------------------------------
void PlayerManager::BroadcastEvent(EEvent leEvent, void* lpEventData)
{
    for (s32 liIndex = 0; liIndex < miNumEventCallbacks; ++liIndex)
    {
        if (maEventCallbacks[liIndex].mCallback != 0)
        {
            maEventCallbacks[liIndex].mCallback(leEvent, lpEventData,
                                                maEventCallbacks[liIndex].mpUserData);
        }
    }
}

void PlayerManager::RegisterEventCallback(EventCallbackFunction lpfCallback, void* lpUserData)
{
    CGS_ASSERT_AT(miNumEventCallbacks < KI_NUM_EVENT_CALLBACKS, "Too many callbacks registered",
                  CGS_ARTIST_FILE, 1687 /* ARTIST 0x82873F74 */);
    maEventCallbacks[miNumEventCallbacks].mCallback  = lpfCallback;
    maEventCallbacks[miNumEventCallbacks].mpUserData = lpUserData;
    ++miNumEventCallbacks;
}

// ---- player counts -------------------------------------------------------------------
// Active players without a network player.
s32 PlayerManager::GetNumberLocalPlayers() const
{
    s32 liNumLocalPlayers = 0;
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        if (maActivePlayers[liIndex].GetNetworkPlayer() == 0)
        {
            ++liNumLocalPlayers;
        }
    }
    return liNumLocalPlayers;
}

// Every active player, or only local players and finalised remote players.
s32 PlayerManager::GetTotalNumberPlayers(EPlayersToConsider leConsider) const
{
    s32 liNumPlayers = 0;
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        bool lbPlayerFinalised = true;
        if (maActivePlayers[liIndex].GetNetworkPlayer() != 0)
        {
            lbPlayerFinalised = mConnectionManager.GetConnectionStatus(
                                    maActivePlayers[liIndex].GetPlayerID()) == E_CONNECTION_SUCCESS;
        }
        if (leConsider == E_CONSIDER_ALL_PLAYERS || lbPlayerFinalised)
        {
            ++liNumPlayers;
        }
    }
    return liNumPlayers;
}

// Remote players only, all of them or only the finalised ones.
s32 PlayerManager::GetNumberNetworkPlayers(EPlayersToConsider leConsider) const
{
    s32 liNumNetworkPlayers = 0;
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        if (maActivePlayers[liIndex].GetNetworkPlayer() == 0)
        {
            continue;
        }
        const bool lbPlayerFinalised = mConnectionManager.GetConnectionStatus(
                                           maActivePlayers[liIndex].GetPlayerID()) == E_CONNECTION_SUCCESS;
        if (leConsider == E_CONSIDER_ALL_PLAYERS || lbPlayerFinalised)
        {
            ++liNumNetworkPlayers;
        }
    }
    return liNumNetworkPlayers;
}

bool PlayerManager::IsPlayerFinalised(NetworkPlayerID lPlayerID) const
{
    return mConnectionManager.GetConnectionStatus(lPlayerID) == E_CONNECTION_SUCCESS;
}

// ---- IsPlayerTurnToSendRoundRobinMessage ---------------------------------------------
// Remote players take turns: each finalised remote player owns one frame in a cycle of
// seven slots of liFrameGapBetweenMessages frames (the caller's gap, else 1 in game and 3
// outside it), indexed by its connection index with the local player's slot squeezed out.
bool PlayerManager::IsPlayerTurnToSendRoundRobinMessage(NetworkPlayerID lPlayerID, bool lbInGame,
                                                        s32 liFrameOffset)
{
    if (IsLocalPlayer(lPlayerID))
    {
        return false;
    }
    if (mConnectionManager.GetConnectionStatus(lPlayerID) != E_CONNECTION_SUCCESS)
    {
        return false;
    }

    CGS_ASSERT(GetNumberNetworkPlayers(E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED) > 0, CGS_ASSERT_MESSAGE_AT(
        "GetNumberNetworkPlayers() > 0",
        CGS_ARTIST_FILE, 1946 /* ARTIST 0x8288FFD4 */));

    const s32 liConnectionIndex   = GetMenuDataByID(lPlayerID)->miConnectionIndex;
    s32       liAmountToDecrement = 0;

    // The walk over the local players (at most one: once lLocalPlayerID holds the local
    // player's id the walk is exhausted).
    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    while (lLocalPlayerID == K_INVALID_PLAYER_ID && GetNextLocalPlayerID(&lLocalPlayerID))
    {
        CGS_ASSERT(GetMenuDataByID(lLocalPlayerID), CGS_ASSERT_MESSAGE_AT(
            "No menu data for the local player!",
            CGS_ARTIST_FILE, 1959 /* ARTIST 0x82890090 */));
        if (GetMenuDataByID(lLocalPlayerID)->miConnectionIndex < liConnectionIndex)
        {
            ++liAmountToDecrement;
        }
    }

    const s32 liRemotePlayerIndex = liConnectionIndex - liAmountToDecrement;

    s32 liFrameGapBetweenMessages;
    if (liFrameOffset == 0)
    {
        liFrameGapBetweenMessages = lbInGame ? 1 : 3;
    }
    else
    {
        liFrameGapBetweenMessages = liFrameOffset;
    }

    CGS_ASSERT(mpTimeManager, CGS_ASSERT_MESSAGE_AT(
        "mpTimeManager",
        CGS_ARTIST_FILE, 1984 /* ARTIST 0x82890140 */));
    const s32 liSeriesNum = static_cast<s32>(
        mpTimeManager->GetFrameCount() %
        static_cast<u32>(liFrameGapBetweenMessages * (KI_MAX_PLAYERS - 1)));
    return liSeriesNum == liRemotePlayerIndex;
}

// ---- AcceptMessage -------------------------------------------------------------------
// A reliable message arrived: queue an ack for it unless an identical ack is already
// pending (same type, frame, player pair and game id).
void PlayerManager::AcceptMessage(Message* lpMessage)
{
    if (!lpMessage->IsReliable())
    {
        return;
    }

    const MessageWithPlayerIDs* lpMessageWithIDs = static_cast<const MessageWithPlayerIDs*>(lpMessage);
    const NetworkPlayerID lSendPlayerID = lpMessageWithIDs->mSendingPlayerID;
    const NetworkPlayerID lRecvPlayerID = lpMessageWithIDs->mRecvingPlayerID;

    for (s32 i = 0; i < KI_MAX_ACKS_TO_BUFFER; ++i)
    {
        const SignalMessage& lAck = maAckMessage[i];
        if (lAck.IsMessageValid() && lAck.GetType() == lpMessage->GetType() &&
            lAck.mu16Frame == lpMessage->mu16Frame &&
            lAck.GetSendingPlayerID() == lSendPlayerID &&
            lAck.GetRecvingPlayerID() == lRecvPlayerID &&
            lAck.GetGameID() == lpMessage->GetGameID())
        {
            return;
        }
    }

    SignalMessage* lpAckMsg = 0;
    for (s32 i = 0; i < KI_MAX_ACKS_TO_BUFFER; ++i)
    {
        if (!maAckMessage[i].IsMessageValid())
        {
            lpAckMsg = &maAckMessage[i];
            break;
        }
    }

    if (lpAckMsg == 0)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "WARNING: run out of acks to use...\n";
        return;
    }

    CGS_ASSERT_AT(lRecvPlayerID >= 0, "lRecvPlayerID >= 0", CGS_ARTIST_FILE, 970 /* ARTIST 0x8287357C */);
    lpAckMsg->PrepareAck(lpMessage, lSendPlayerID, lRecvPlayerID);
    CGS_ASSERT_AT(lpAckMsg->GetSendingPlayerID() >= 0, "lpAckMsg->GetSendingPlayerID() >= 0",
                  CGS_ARTIST_FILE, 981 /* ARTIST 0x828735BC */);
    CGS_ASSERT_AT(lpAckMsg->GetRecvingPlayerID() >= 0, "lpAckMsg->GetRecvingPlayerID() >= 0",
                  CGS_ARTIST_FILE, 982 /* ARTIST 0x828735E8 */);
}

// ---- ThrowAwayMessage ----------------------------------------------------------------
// A reliable message is being dropped: queue a nack for it in the first free slot.
void PlayerManager::ThrowAwayMessage(Message* lpMessage)
{
    if (!lpMessage->IsReliable())
    {
        return;
    }

    SignalMessage* lpNackMsg = 0;
    for (s32 i = 0; i < KI_MAX_NACKS_TO_BUFFER; ++i)
    {
        if (!maNackMessage[i].IsMessageValid())
        {
            lpNackMsg = &maNackMessage[i];
            break;
        }
    }

    if (lpNackMsg == 0)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "WARNING: run out of nacks to use...\n";
        return;
    }

    const MessageWithPlayerIDs* lpMessageWithIDs = static_cast<const MessageWithPlayerIDs*>(lpMessage);
    const NetworkPlayerID lSendPlayerID = lpMessageWithIDs->mSendingPlayerID;
    const NetworkPlayerID lRecvPlayerID = lpMessageWithIDs->mRecvingPlayerID;
    CGS_ASSERT(lSendPlayerID != -1, CGS_ASSERT_MESSAGE_AT(
        "lSendPlayerID != -1",
        CGS_ARTIST_FILE, 1036 /* ARTIST 0x828736C4 */));

    lpNackMsg->PrepareNack(lpMessage, lSendPlayerID, lRecvPlayerID);

    CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "Sending a nack for msg type " << static_cast<s32>(lpMessage->GetType())
                               << " frame " << static_cast<s32>(lpMessage->mu16Frame)
                               << " to player calling themselves index " << lSendPlayerID << "\n";
    CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "We are calling ourselves " << lRecvPlayerID << "\n";
}

// ---- CheckForNack --------------------------------------------------------------------
// A received nack frees the buffered reliable message it answers.
bool PlayerManager::CheckForNack(Message* lpMessage)
{
    if ((lpMessage->mx8Flags & KX8_FLAGS_NACK) == 0)
    {
        return false;
    }

    *CgsDev::Log::gpDebugPrint << "Received a nack for msg type " << static_cast<s32>(lpMessage->GetType())
                               << " frame " << static_cast<s32>(lpMessage->mu16Frame) << "\n";
    mReliableMessageManager.RemoveBufferedReliableMessage(static_cast<SignalMessage*>(lpMessage));
    return true;
}

}

// FOLDED FROM CgsPlayerManager_wN2_03.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // ReceiveMessage, GetConnectionData
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"          // ReceiveFrom, ConnectionData
#include "GameShared/GameClasses/Network/Packeting/CgsCompressionAndEncryptionUtils.h" // ExtractSendingPlayerID, header layout
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                          // IPAddressIntToString
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint

#include "types.hpp"   // uintptr_t

// CgsNetwork::PlayerManager::ReceiveMessages -- the per-frame receive pump.

namespace CgsNetwork
{

namespace
{
    // Packets received during the previous pump (reset every pump, logged when high).
    s32 s_iNumMessagesReceivedThisFrame = 0;

    const s32 KI_MANY_MESSAGES_RECEIVED_THIS_FRAME = 25;
}

// ---- ReceiveMessages -----------------------------------------------------------------
// For every remote player with a live link, drain the adapter and hand each packet to the
// player. While the disk is not accessible packets are read and dropped. A reliable packet
// whose header names a different sender than the player it arrived from is dumped to the
// log (the packet's header and every known player's connection data) and asserted.
void PlayerManager::ReceiveMessages()
{
    if (mpNetworkAdapter == 0)
    {
        return;
    }

    if (s_iNumMessagesReceivedThisFrame > KI_MANY_MESSAGES_RECEIVED_THIS_FRAME)
    {
        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput << "We received " << s_iNumMessagesReceivedThisFrame
                                   << " messages this frame, which is rather high\n";
    }
    s_iNumMessagesReceivedThisFrame = 0;

    NetworkPlayerID lPlayerId = K_INVALID_PLAYER_ID;
    while (GetNextPlayerID(&lPlayerId, E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpNetPlayer = GetPlayerByID(lPlayerId);
        if (lpNetPlayer == 0)
        {
            continue;
        }

        ConnectionData lConnectionData = lpNetPlayer->GetConnectionData();
        if (!lConnectionData.IsValid())
        {
            continue;
        }

        for (;;)
        {
            void* lpMsgData;
            const s32 liBytesRecvd = mpNetworkAdapter->ReceiveFrom(&lpMsgData, lConnectionData);
            if (liBytesRecvd <= 0)
            {
                break;
            }
            if (!mbDiskAccessible)
            {
                continue;
            }

            ++s_iNumMessagesReceivedThisFrame;

            u8* lpu8MsgData = static_cast<u8*>(lpMsgData);
            const NetworkPlayerID lSendingPlayerIDFromMessageHeader =
                CompressionAndEncryptionUtils::ExtractSendingPlayerID(lpu8MsgData, liBytesRecvd);
            if (lSendingPlayerIDFromMessageHeader != K_INVALID_PLAYER_ID)
            {
                if (lSendingPlayerIDFromMessageHeader != lpNetPlayer->GetPlayerID())
                {
                    s32 liPlayerIndex = 0;
                    NetworkPlayerID lOutputPlayerId = K_INVALID_PLAYER_ID;
                    const CompressionAndEncryptionUtils::PackedPacketHeader* lpHeader =
                        reinterpret_cast<const CompressionAndEncryptionUtils::PackedPacketHeader*>(lpu8MsgData);
                    const u8* lpPackedTypes =
                        ((lpHeader->mx8Flags & KX8_FLAGS_RELIABLE) == KX8_FLAGS_RELIABLE)
                            ? lpu8MsgData + sizeof(CompressionAndEncryptionUtils::ReliablePackedPacketHeader)
                            : lpu8MsgData + sizeof(CompressionAndEncryptionUtils::PackedPacketHeader);
                    static const char* const KPAC_CONNECTION_TYPE_STRING[2] = { "Peer-peer", "GameServer" };

                    CgsDev::StrStreamBase& lLog = CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput;
                    lLog << "Player ID in message doesn't match ID of player received from!\n";
                    lLog << "==============================================================\n\n";
                    lLog << "Message contains:\n";
                    lLog << "Sending player ID of " << lSendingPlayerIDFromMessageHeader
                         << " we expected " << lpNetPlayer->GetPlayerID() << "\n";
                    lLog << "Number of signal messages: " << static_cast<s32>(lpHeader->mu8NumSignalMsgs) << "\n";
                    lLog << "Number of normal messages: " << static_cast<s32>(lpHeader->mu8NumTypes) << "\n";
                    lLog << "Message types:\n";
                    for (u8 lu8MessageIndex = 0; lu8MessageIndex < lpHeader->mu8NumTypes; ++lu8MessageIndex)
                    {
                        lLog << static_cast<s32>(lpPackedTypes[lu8MessageIndex]) << "\n";
                    }
                    lLog << "\n";
                    lLog << "==============================================================\n\n";
                    lLog << "Active players: " << miNumActivePlayers << "\n\n";

                    while (GetNextPlayerID(&lOutputPlayerId, E_CONSIDER_ALL_PLAYERS))
                    {
                        NetworkPlayer*  lpOutputNetPlayer = GetPlayerByID(lOutputPlayerId);
                        PlayerMenuData* lpOutputMenuData  = GetMenuDataByID(lOutputPlayerId);

                        lLog << "Player: " << liPlayerIndex << "\n";
                        lLog << "Name: " << lpOutputMenuData->macName << "\n";
                        lLog << "Player ID: " << lOutputPlayerId << "\n";
                        lLog << "Local Player: " << (lpOutputNetPlayer != 0 ? "no" : "yes") << "\n";

                        if (lpOutputNetPlayer != 0)
                        {
                            ConnectionData lOutputConnectionData = lpOutputNetPlayer->GetConnectionData();
                            if (!lOutputConnectionData.IsValid())
                            {
                                lLog << "Connection data not valid\n";
                            }
                            else
                            {
                                char lacIPAddress[17];
                                IPAddressIntToString(lacIPAddress, lOutputConnectionData.miIPAddress);
                                lLog << "External IP Address: " << lacIPAddress << "\n";
                                IPAddressIntToString(lacIPAddress, lOutputConnectionData.miLocalIPAddress);
                                lLog << "Internal IP Address: " << lacIPAddress << "\n";
                                lLog << "Game Port: " << lOutputConnectionData.muGamePort << "\n";
                                lLog << "Voip Port: " << lOutputConnectionData.muVoipPort << "\n";
                                lLog << "Game Local Port: " << lOutputConnectionData.muLocalGamePort << "\n";
                                lLog << "Game Mangle Port: " << lOutputConnectionData.muMnglGamePort << "\n";
                                lLog << "Voip Local Port: " << lOutputConnectionData.muLocalVoipPort << "\n";
                                lLog << "Voip Mangle Port: " << lOutputConnectionData.muMnglVoipPort << "\n";
                                // +0x30 holds the peer's DirtyAddrT text.
                                lLog << "DirtyAddrT: "
                                     << reinterpret_cast<const char*>(lOutputConnectionData.maReserved30) << "\n";
                                lLog << "Game connection type: "
                                     << KPAC_CONNECTION_TYPE_STRING[lOutputConnectionData.meGameConnectionType] << "\n";
                                lLog << "Voip connection type: "
                                     << KPAC_CONNECTION_TYPE_STRING[lOutputConnectionData.meVoipConnectionType] << "\n";
                                lLog << "NetGameLinkRef address " << CgsDev::E_PRINTMODE_HEX
                                     << static_cast<u32>(reinterpret_cast<uintptr_t>(lOutputConnectionData.mpNetGameLink))
                                     << CgsDev::E_PRINTMODE_HEXONCE << "\n";
                            }
                        }

                        lLog << "\n----------------------------------------------------------\n\n";
                        ++liPlayerIndex;
                    }

                    lLog << "==============================================================\n\n";
                }

                CGS_ASSERT(lSendingPlayerIDFromMessageHeader == lpNetPlayer->GetPlayerID(),
                           "lSendingPlayerIDFromMessageHeader == lpNetPlayer->GetPlayerID()");
            }

            lpNetPlayer->ReceiveMessage(lpu8MsgData, liBytesRecvd, lpNetPlayer->GetPlayerID());
        }
    }
}

}

// FOLDED FROM CgsPlayerManager_wN2_04.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // GetConnectionData
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "netgamelink.h"                                                // NetGameLinkStatus

// CgsNetwork::PlayerManager -- the bandwidth totals the debug HUD shows: one per-second rate
// from every remote player's game link, summed.

namespace CgsNetwork
{

// ---- GetTotalBytesSent ---------------------------------------------------------------
// Bytes per second handed to the transport (payload plus link framing).
u32 PlayerManager::GetTotalBytesSent()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayer",
            CGS_ARTIST_FILE, 2134 /* ARTIST 0x828741B8 */));

        luTotal += lpNetworkPlayer->GetBytesSentToNetwork();
    }
    return luTotal;
}

// ---- GetTotalBytesSentToDirtySock ----------------------------------------------------
// Game payload bytes per second.
u32 PlayerManager::GetTotalBytesSentToDirtySock()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayer",
            CGS_ARTIST_FILE, 2162 /* ARTIST 0x828742E0 */));

        luTotal += lpNetworkPlayer->GetBytesSentToDirtySock();
    }
    return luTotal;
}

// ---- GetTotalBytesSentWithOverhead ---------------------------------------------------
// Bytes per second on the wire.
u32 PlayerManager::GetTotalBytesSentWithOverhead()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, CGS_ASSERT_MESSAGE_AT(
            "lpNetworkPlayer",
            CGS_ARTIST_FILE, 2191 /* ARTIST 0x82874408 */));

        luTotal += lpNetworkPlayer->GetBytesSentToNetworkWithOverhead();
    }
    return luTotal;
}

}

#undef CGS_ARTIST_FILE
