#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // NetworkPlayer / PlayerMenuData
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"      // SignalMessage::PrepareAck / PrepareNack
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                           // UsernameCompare
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"                       // TimeManager::GetFrameCount
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"        // GetConnectionComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"                 // GetLobbyAPIRef
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"     // ServerInterfaceConnection
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"            // CgsDev::PerfMonCpu
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "lobbyapi.h"                                                                  // LobbyApiDisconnect

#include <cstdint>   // intptr_t
#include <cstring>   // strlen / strncpy

#include "netconn.h"   // NetConnStatus

// CgsNetwork::PlayerManager -- the player registry proper: adding and removing players, the
// active / inactive table moves, the id walks and lookups, host identity, the event
// callbacks, the connection-manager callbacks, the ack / nack filters for received
// messages and the round-robin send slot.

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
        *CgsDev::Log::gpDebugPrint << "CgsNetwork::PlayerManager::AddPlayer"
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
    CGS_ASSERT(liPlayerIndex < miNumActivePlayers, "liPlayerIndex < miNumActivePlayers");

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
        if (lpNetworkPlayer != nullptr && !lpNetworkPlayer->mbNetworkPlayerPaused)
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
    CGS_ASSERT(lpPlayerManager, "lpPlayerManager");

    lpPlayerManager->BroadcastEvent(E_EVENT_PLAYER_DISCONNECTED,
                                    reinterpret_cast<void*>(static_cast<intptr_t>(lPlayerID)));

    NetworkPlayer* lpNetworkPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
    CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");
    lpNetworkPlayer->SetDisconnected();

    if (lpPlayerManager->AreAnyPlayersConnected())
    {
        return;
    }

    CGS_ASSERT(lpPlayerManager->mpServerInterface, "lpPlayerManager->mpServerInterface");
    ServerInterfaceConnection* lpServerInterfaceConnection = static_cast<ServerInterfaceConnection*>(
        lpPlayerManager->mpServerInterface->GetConnectionComponent());
    CGS_ASSERT(lpServerInterfaceConnection, "lpServerInterfaceConnection");

    if (NetConnStatus(KI_NETCONN_STATUS_PLUG, 0, nullptr, 0) == 0)
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

    if (lpPlayerManager->mpfConnectionFinalisedCallback != nullptr)
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
    CGS_ASSERT(liPlayerIndex < miNumActivePlayers, "liPlayerIndex < miNumActivePlayers");

    NetworkPlayer* lpNetworkPlayer = maActivePlayers[liPlayerIndex].GetNetworkPlayer();
    if (lpNetworkPlayer != nullptr)
    {
        lpNetworkPlayer->Release();
    }

    maInactivePlayers[miNumInactivePlayers] = maActivePlayers[liPlayerIndex];
    ++miNumInactivePlayers;

    for (; liPlayerIndex < miNumActivePlayers - 1; ++liPlayerIndex)
    {
        CGS_ASSERT((liPlayerIndex + 1) < KI_MAX_PLAYERS, "(liPlayerIndex + 1) < KI_MAX_PLAYERS");
        maActivePlayers[liPlayerIndex] = maActivePlayers[liPlayerIndex + 1];
        maActivePlayers[liPlayerIndex].GetMenuData()->miConnectionIndex = liPlayerIndex;
    }

    maActivePlayers[miNumActivePlayers - 1].SetNetworkPlayer(nullptr);
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
        if (maInactivePlayers[liIndex].GetNetworkPlayer() == nullptr)
        {
            maActivePlayers[liConnectionIndex] = maInactivePlayers[liIndex];
            maInactivePlayers[liIndex]         = maInactivePlayers[miNumInactivePlayers - 1];
            --miNumInactivePlayers;
            ++miNumActivePlayers;
            return &maActivePlayers[liConnectionIndex];
        }
    }

    CGS_ASSERT(false, "Couldn't find a free inactive local player");
    return nullptr;
}

// ---- AssignActiveNetworkPlayer -------------------------------------------------------
// As above, for the first inactive slot that carries a network player.
PlayerData* PlayerManager::AssignActiveNetworkPlayer(s32 liConnectionIndex)
{
    for (s32 liIndex = 0; liIndex < miNumInactivePlayers; ++liIndex)
    {
        if (maInactivePlayers[liIndex].GetNetworkPlayer() != nullptr)
        {
            maActivePlayers[liConnectionIndex] = maInactivePlayers[liIndex];
            maInactivePlayers[liIndex]         = maInactivePlayers[miNumInactivePlayers - 1];
            --miNumInactivePlayers;
            ++miNumActivePlayers;
            return &maActivePlayers[liConnectionIndex];
        }
    }

    CGS_ASSERT(false, "Couldn't find a free inactive network player");
    return nullptr;
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
            if (maActivePlayers[liIndex].GetNetworkPlayer() == nullptr ||
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
        if (maActivePlayers[liIndex].GetNetworkPlayer() != nullptr)
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
    for (s32 i = 0; i < miNumActivePlayers; ++i)
    {
        if (maActivePlayers[i].GetPlayerID() == lPlayerID)
        {
            return maActivePlayers[i].GetNetworkPlayer();
        }
    }
    return nullptr;
}

// ---- GetPlayerByName -----------------------------------------------------------------
// The first active network player whose name matches under the lobby name comparison.
NetworkPlayer* PlayerManager::GetPlayerByName(const char* lpcName)
{
    for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
    {
        NetworkPlayer* lpNetworkPlayer = maActivePlayers[liIndex].GetNetworkPlayer();
        if (lpNetworkPlayer != nullptr && UsernameCompare(lpNetworkPlayer->GetName(), lpcName) == 0)
        {
            return lpNetworkPlayer;
        }
    }
    return nullptr;
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
    return nullptr;
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
    CGS_ASSERT((lLocalPlayerID != K_INVALID_PLAYER_ID) || (0 == miNumActivePlayers),
               "(lLocalPlayerID != K_INVALID_PLAYER_ID) || (0 == miNumActivePlayers)");
    return mHostPlayerID == lLocalPlayerID;
}

// ---- AckNeedsSending / NackNeedsSending ----------------------------------------------
// Whether ack / nack slot liIndex holds a pending signal from lPlayerID.
bool PlayerManager::AckNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
    CGS_ASSERT(liIndex < KI_MAX_ACKS_TO_BUFFER, "liIndex < KI_MAX_ACKS_TO_BUFFER");

    if (!maAckMessage[liIndex].IsMessageValid())
    {
        return false;
    }
    return maAckMessage[liIndex].GetSendingPlayerID() == lPlayerID;
}

bool PlayerManager::NackNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const
{
    CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
    CGS_ASSERT(liIndex < KI_MAX_NACKS_TO_BUFFER, "liIndex < KI_MAX_NACKS_TO_BUFFER");

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
        if (maEventCallbacks[liIndex].mCallback != nullptr)
        {
            maEventCallbacks[liIndex].mCallback(leEvent, lpEventData,
                                                maEventCallbacks[liIndex].mpUserData);
        }
    }
}

void PlayerManager::RegisterEventCallback(EventCallbackFunction lpfCallback, void* lpUserData)
{
    CGS_ASSERT(miNumEventCallbacks < KI_NUM_EVENT_CALLBACKS, "Too many callbacks registered");
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
        if (maActivePlayers[liIndex].GetNetworkPlayer() == nullptr)
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
        if (maActivePlayers[liIndex].GetNetworkPlayer() != nullptr)
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
        if (maActivePlayers[liIndex].GetNetworkPlayer() == nullptr)
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

    CGS_ASSERT(GetNumberNetworkPlayers(E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED) > 0,
               "GetNumberNetworkPlayers() > 0");

    const s32 liConnectionIndex   = GetMenuDataByID(lPlayerID)->miConnectionIndex;
    s32       liAmountToDecrement = 0;

    // The walk over the local players (at most one: once lLocalPlayerID holds the local
    // player's id the walk is exhausted).
    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    while (lLocalPlayerID == K_INVALID_PLAYER_ID && GetNextLocalPlayerID(&lLocalPlayerID))
    {
        CGS_ASSERT(GetMenuDataByID(lLocalPlayerID), "No menu data for the local player!");
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

    CGS_ASSERT(mpTimeManager, "mpTimeManager");
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

    SignalMessage* lpAckMsg = nullptr;
    for (s32 i = 0; i < KI_MAX_ACKS_TO_BUFFER; ++i)
    {
        if (!maAckMessage[i].IsMessageValid())
        {
            lpAckMsg = &maAckMessage[i];
            break;
        }
    }

    if (lpAckMsg == nullptr)
    {
        *CgsDev::Log::gpDebugPrint << "WARNING: run out of acks to use...\n";
        return;
    }

    CGS_ASSERT(lRecvPlayerID >= 0, "lRecvPlayerID >= 0");
    lpAckMsg->PrepareAck(lpMessage, lSendPlayerID, lRecvPlayerID);
    CGS_ASSERT(lpAckMsg->GetSendingPlayerID() >= 0, "lpAckMsg->GetSendingPlayerID() >= 0");
    CGS_ASSERT(lpAckMsg->GetRecvingPlayerID() >= 0, "lpAckMsg->GetRecvingPlayerID() >= 0");
}

// ---- ThrowAwayMessage ----------------------------------------------------------------
// A reliable message is being dropped: queue a nack for it in the first free slot.
void PlayerManager::ThrowAwayMessage(Message* lpMessage)
{
    if (!lpMessage->IsReliable())
    {
        return;
    }

    SignalMessage* lpNackMsg = nullptr;
    for (s32 i = 0; i < KI_MAX_NACKS_TO_BUFFER; ++i)
    {
        if (!maNackMessage[i].IsMessageValid())
        {
            lpNackMsg = &maNackMessage[i];
            break;
        }
    }

    if (lpNackMsg == nullptr)
    {
        *CgsDev::Log::gpDebugPrint << "WARNING: run out of nacks to use...\n";
        return;
    }

    const MessageWithPlayerIDs* lpMessageWithIDs = static_cast<const MessageWithPlayerIDs*>(lpMessage);
    const NetworkPlayerID lSendPlayerID = lpMessageWithIDs->mSendingPlayerID;
    const NetworkPlayerID lRecvPlayerID = lpMessageWithIDs->mRecvingPlayerID;
    CGS_ASSERT(lSendPlayerID != -1, "lSendPlayerID != -1");

    lpNackMsg->PrepareNack(lpMessage, lSendPlayerID, lRecvPlayerID);

    *CgsDev::Log::gpDebugPrint << "Sending a nack for msg type " << static_cast<s32>(lpMessage->GetType())
                               << " frame " << static_cast<s32>(lpMessage->mu16Frame)
                               << " to player calling themselves index " << lSendPlayerID << "\n";
    *CgsDev::Log::gpDebugPrint << "We are calling ourselves " << lRecvPlayerID << "\n";
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
