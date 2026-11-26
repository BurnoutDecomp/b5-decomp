#include "CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h" // mNetStreamLogChannelOutput (0x82F335C8)
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsPlayersConnectionManager.cpp"

namespace CgsNetwork
{
// The console ctor only seeds the embedded members of the seven entries: the test-connection
// and connection-status message objects and the test timestamp (CgsSystem::Time's own ctor).
PlayersConnectionManager::PlayersConnectionManager()
{
}
}

// FOLDED FROM CgsPlayersConnectionManager_wN2_01.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // GetNextLocalPlayerID
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"        // GetConnAPIRef
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                            // CgsDev::Log::gpDebugPrint

#include "connapi.h"                                                             // ConnApiGetClientList

// CgsNetwork::PlayersConnectionManager -- the per-frame pump and the connection-state
// queries the lobby and launch flow poll.

namespace CgsNetwork
{

// ---- Update --------------------------------------------------------------------------
// While ConnAPI has a client list: sync the game id, reconcile the player list, advance the
// test-connection handshakes and broadcast our connection status.
void PlayersConnectionManager::Update(const CgsSystem::TimerStatus* lpTimerStatus,
                                      u16 lu16CurrentFrame)
{
    DirtySock::ConnApiRefT* lpConnApi = mpServerInterface->GetConnAPIRef();
    if (lpConnApi == 0)
    {
        return;
    }

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    if (lpClientList == 0)
    {
        return;
    }

    CheckAndUpdateGameID();
    UpdatePlayerList(lpClientList, lpTimerStatus, lu16CurrentFrame);
    CheckTestConnectionMessageStatus(lpTimerStatus, lu16CurrentFrame);
    SendConnectionStatusMessages(lu16CurrentFrame);
}

// ---- GetConnectionStatus -------------------------------------------------------------
// The handshake state of a remote player (never ask about ourselves).
EConnectionStatus PlayersConnectionManager::GetConnectionStatus(NetworkPlayerID lPlayerID) const
{
    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    CGS_ASSERT_AT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 1574);
    CGS_ASSERT_AT(lPlayerID != lLocalPlayerID, "lPlayerID != lLocalPlayerID", CGS_ARTIST_FILE, 1575);

    const ConnectionDataEntry* lpEntry = GetEntry(lPlayerID);
    CGS_ASSERT_AT(lpEntry, "lpEntry", CGS_ARTIST_FILE, 1580);
    return lpEntry->mPlayerConnectionData.meConnectionStatus;
}

// ---- HavePlayersFailedToConnect ------------------------------------------------------
// True when either side of the (lPlayerID1, lPlayerID2) pair reports E_CONNECTION_FAILURE:
// our own entry for the other player when one of them is us, else what each player last
// told us about the other.
bool PlayersConnectionManager::HavePlayersFailedToConnect(NetworkPlayerID lPlayerID1,
                                                          NetworkPlayerID lPlayerID2) const
{
    CGS_ASSERT_AT(lPlayerID1 != K_INVALID_PLAYER_ID,
        "lPlayerID1 != K_INVALID_PLAYER_ID",
        CGS_ARTIST_FILE, 732 /* ARTIST 0x82884B4C */);
    CGS_ASSERT_AT(lPlayerID2 != K_INVALID_PLAYER_ID,
        "lPlayerID2 != K_INVALID_PLAYER_ID",
        CGS_ARTIST_FILE, 733 /* ARTIST 0x82884B70 */);

    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    // Player 1's view of player 2.
    if (lPlayerID1 == lLocalPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
        {
            const PlayerConnectionData& lData = maConnectionDataEntry[liIndex].mPlayerConnectionData;
            if (lData.mPlayerID == lPlayerID2)
            {
                if (lData.meConnectionStatus == E_CONNECTION_FAILURE)
                {
                    return true;
                }
                break;
            }
        }
    }
    else
    {
        const ConnectionDataEntry* lpPlayer1Entry = GetEntry(lPlayerID1);
        CGS_ASSERT_AT(lpPlayer1Entry,
            "lpPlayer1Entry",
            CGS_ARTIST_FILE, 758 /* ARTIST 0x82884C0C */);
        for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
        {
            const PlayerConnectionData& lData = lpPlayer1Entry->maLastFinaliseStateRecvd[liIndex];
            if (lData.mPlayerID == lPlayerID2)
            {
                if (lData.meConnectionStatus == E_CONNECTION_FAILURE)
                {
                    return true;
                }
                break;
            }
        }
    }

    // Player 2's view of player 1.
    if (lPlayerID2 == lLocalPlayerID)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
        {
            const PlayerConnectionData& lData = maConnectionDataEntry[liIndex].mPlayerConnectionData;
            if (lData.mPlayerID == lPlayerID1)
            {
                return lData.meConnectionStatus == E_CONNECTION_FAILURE;
            }
        }
        return false;
    }

    const ConnectionDataEntry* lpPlayer2Entry = GetEntry(lPlayerID2);
    CGS_ASSERT_AT(lpPlayer2Entry,
        "lpPlayer2Entry",
        CGS_ARTIST_FILE, 795 /* ARTIST 0x82884CD4 */);
    for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
    {
        const PlayerConnectionData& lData = lpPlayer2Entry->maLastFinaliseStateRecvd[liIndex];
        if (lData.mPlayerID == lPlayerID1)
        {
            return lData.meConnectionStatus == E_CONNECTION_FAILURE;
        }
    }
    return false;
}

// ---- AreAllConnectionsSuccessful -----------------------------------------------------
// Every known player must be E_CONNECTION_SUCCESS from our side, and every known player's
// last reported state for every other known player must be E_CONNECTION_SUCCESS too. The
// first gap is logged and fails the check. (The console also scans the registry's active
// players for each id in the inner loop; the scan's result is never used.)
bool PlayersConnectionManager::AreAllConnectionsSuccessful() const
{
    NetworkPlayerID laPlayerID[KI_MAX_CONNECTION_ENTRIES];
    s32             laEntryIndex[KI_MAX_CONNECTION_ENTRIES];
    s32             liNumPlayers = 0;

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        const PlayerConnectionData& lData = maConnectionDataEntry[liIndex].mPlayerConnectionData;
        if (lData.mPlayerID == K_INVALID_PLAYER_ID)
        {
            continue;
        }

        laPlayerID[liNumPlayers]   = lData.mPlayerID;
        laEntryIndex[liNumPlayers] = liIndex;
        ++liNumPlayers;

        if (lData.meConnectionStatus != E_CONNECTION_SUCCESS)
        {
            CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
                << "PlayersConnectionManager::AreAllConnectionsSuccessful: We haven't connected to player "
                << laPlayerID[liNumPlayers - 1] << "\n";
            return false;
        }
    }

    for (s32 liPlayerA = 0; liPlayerA < liNumPlayers; ++liPlayerA)
    {
        const ConnectionDataEntry& lEntryA = maConnectionDataEntry[laEntryIndex[liPlayerA]];
        for (s32 liPlayerB = 0; liPlayerB < liNumPlayers; ++liPlayerB)
        {
            if (laPlayerID[liPlayerB] == lEntryA.mPlayerConnectionData.mPlayerID)
            {
                continue;
            }

            s32 liIndex = 0;
            for (; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
            {
                if (lEntryA.maLastFinaliseStateRecvd[liIndex].mPlayerID == laPlayerID[liPlayerB])
                {
                    if (lEntryA.maLastFinaliseStateRecvd[liIndex].meConnectionStatus != E_CONNECTION_SUCCESS)
                    {
                        CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
                            << "PlayersConnectionManager::AreAllConnectionsSuccessful: "
                            << laPlayerID[liPlayerA] << " hasn't connected to player "
                            << laPlayerID[liPlayerB] << "\n";
                        return false;
                    }
                    break;
                }
            }

            if (liIndex == KI_CONNECTION_STATUS_PLAYER_COUNT)
            {
                CgsNetwork::ServerInterfaceDirtySock::mNetStreamLogChannelOutput
                    << "PlayersConnectionManager::AreAllConnectionsSuccessful: Couldn't find connection entry for "
                    << laPlayerID[liPlayerB] << " in player " << laPlayerID[liPlayerA]
                    << "'s connection list\n";
                return false;
            }
        }
    }

    return true;
}

}

// FOLDED FROM CgsPlayersConnectionManager_wN2_02.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // GetPlayerByID / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // RegisterMessageType / UnRegisterMessageType
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"        // GetGameComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"              // TimerStatus::GetTime
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// CgsNetwork::PlayersConnectionManager -- the connection-entry bookkeeping (add / remove a
// peer, register its two message types), the test-connection handshake, the connection
// status broadcast and the message callbacks the network players invoke.

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsPlayersConnectionManager.cpp".

namespace CgsNetwork
{

// How long a peer may take to acknowledge our test-connection message before the connection
// is declared failed.
const CgsSystem::Time K_TEST_PACKET_TIMEOUT_SECONDS(30.0f);

// The two shared message types this manager registers with every remote network player.
static const s32 KI_E_MESSAGE_TYPE_TEST_CONNECTION   = 4;
static const s32 KI_E_MESSAGE_TYPE_CONNECTION_STATUS = 5;

// ---- CheckAndUpdateGameID ------------------------------------------------------------
// Push the lobby game id (0 when we are not in a game) into the player registry when it
// has changed.
void PlayersConnectionManager::CheckAndUpdateGameID()
{
    ServerInterfaceGames* lpGames =
        static_cast<ServerInterfaceGames*>(mpServerInterface->GetGameComponent());

    u8 lu8GameID = static_cast<u8>(lpGames->GetGameID());
    if (lu8GameID == KU8_INVALID_GAME_ID)
    {
        lu8GameID = 0;
    }

    if (lu8GameID != mpPlayerManager->GetGameID())
    {
        mpPlayerManager->SetGameID(lu8GameID);
    }
}

// ---- AddPlayer -----------------------------------------------------------------------
// A remote player takes the first free connection entry, which is reset for a fresh
// handshake before the registry adds the player and its message types are registered. A
// local player only goes into the registry (no entry, null result).
PlayersConnectionManager::ConnectionDataEntry*
PlayersConnectionManager::AddPlayer(const CgsSystem::TimerStatus* lpTimerStatus,
                                    const char* lpcName, NetworkPlayerID lPlayerID,
                                    s32 liConnectionIndex, bool lbLocal)
{
    ConnectionDataEntry* lpEntry = 0;

    if (!lbLocal)
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
        {
            if (maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID == K_INVALID_PLAYER_ID)
            {
                lpEntry = &maConnectionDataEntry[liIndex];
                break;
            }
        }
        CGS_ASSERT(lpEntry, "lpEntry");

        lpEntry->mPlayerConnectionData.mPlayerID          = lPlayerID;
        lpEntry->mPlayerConnectionData.meConnectionStatus = E_NOT_STARTED;
        for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
        {
            lpEntry->maLastFinaliseStateRecvd[liIndex].mPlayerID          = K_INVALID_PLAYER_ID;
            lpEntry->maLastFinaliseStateRecvd[liIndex].meConnectionStatus = E_NOT_STARTED;
            lpEntry->maLastFinaliseStateSent[liIndex].mPlayerID           = K_INVALID_PLAYER_ID;
            lpEntry->maLastFinaliseStateSent[liIndex].meConnectionStatus  = E_NOT_STARTED;
        }

        lpEntry->mConnectionData.Clear();
        lpEntry->mTestConnectionMessageSend.Construct();
        lpEntry->mTimeTestConnectionSent            = lpTimerStatus->GetTime();
        lpEntry->mbHaveReceivedTestPacketFromPlayer = false;
        lpEntry->mbTestConnectionPacketDelivered    = false;
        lpEntry->mbTestConnectionPacketNacked       = false;
        lpEntry->mConnectionStatusMessageSend.Construct();
    }

    mpPlayerManager->AddPlayer(lpTimerStatus, lpcName, lPlayerID, liConnectionIndex,
                               CgsSystem::E_FRAMERATE_UNKNOWN, lbLocal);

    if (!lbLocal)
    {
        RegisterMessageTypes(lpEntry);
    }

    return lpEntry;
}

// ---- RemovePlayer --------------------------------------------------------------------
// Removing ourselves only drops the registry player and forgets our address; a remote
// player also loses its message types and frees its connection entry.
void PlayersConnectionManager::RemovePlayer(NetworkPlayerID lPlayerID)
{
    NetworkPlayerID lLocalPlayerID = K_INVALID_PLAYER_ID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    if (lPlayerID == lLocalPlayerID)
    {
        mpPlayerManager->RemovePlayer(lPlayerID);
        muLocalPlayerIPAddress = 0;
    }
    else
    {
        ConnectionDataEntry* lpEntry = GetEntry(lPlayerID);
        CGS_ASSERT_AT(lpEntry, "lpEntry", CGS_ARTIST_FILE, 1115);

        UnRegisterMessageTypes(lpEntry);
        mpPlayerManager->RemovePlayer(lPlayerID);
        lpEntry->mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
    }
}

// ---- RegisterMessageTypes ------------------------------------------------------------
// The entry's send / receive message pairs go onto the peer's network player, with this
// manager as the callbacks' user data.
void PlayersConnectionManager::RegisterMessageTypes(ConnectionDataEntry* lpEntry)
{
    NetworkPlayer* lpNetPlayer = mpPlayerManager->FindActiveNetworkPlayer(lpEntry->mPlayerConnectionData.mPlayerID);
    CGS_ASSERT_AT(lpNetPlayer, "lpNetPlayer", CGS_ARTIST_FILE, 1352);

    // Registered lengths are 0x28 and 0x60 console bytes.
    lpNetPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_TEST_CONNECTION, sizeof(TestConnectionMessage),
                                     &lpEntry->mTestConnectionMessageSend,
                                     &lpEntry->mTestConnectionMessageRecv,
                                     TestConnectionMessageArrivedCallback,
                                     TestConnectionMessageDeliveredCallback,
                                     this);
    lpNetPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_CONNECTION_STATUS, sizeof(ConnectionStatusMessage),
                                     &lpEntry->mConnectionStatusMessageSend,
                                     &lpEntry->mConnectionStatusMessageRecv,
                                     ConnectionStatusMessageArrivedCallback,
                                     ConnectionStatusMessageDeliveredCallback,
                                     this);
}

// ---- UnRegisterMessageTypes ----------------------------------------------------------
void PlayersConnectionManager::UnRegisterMessageTypes(ConnectionDataEntry* lpEntry)
{
    NetworkPlayer* lpNetPlayer = mpPlayerManager->FindActiveNetworkPlayer(lpEntry->mPlayerConnectionData.mPlayerID);
    CGS_ASSERT_AT(lpNetPlayer, "lpNetPlayer", CGS_ARTIST_FILE, 1387);

    lpNetPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_TEST_CONNECTION);
    lpNetPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_CONNECTION_STATUS);
}

// ---- CheckTestConnectionMessageStatus ------------------------------------------------
// For every peer still checking its connection: a delivered test message finalises it as a
// success, a nacked one is sent again, and a test outstanding for longer than the timeout
// finalises it as a failure. Both edges report through the finalised callback.
void PlayersConnectionManager::CheckTestConnectionMessageStatus(const CgsSystem::TimerStatus* lpTimerStatus,
                                                                u16 lu16CurrentFrame)
{
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        ConnectionDataEntry*  lpEntry   = &maConnectionDataEntry[liIndex];
        const NetworkPlayerID lPlayerID = lpEntry->mPlayerConnectionData.mPlayerID;
        if (lPlayerID == K_INVALID_PLAYER_ID)
        {
            continue;
        }

        NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetPlayer, "lpNetPlayer");

        if (lpEntry->mPlayerConnectionData.meConnectionStatus != E_CHECKING_CONNECTION)
        {
            continue;
        }

        if (lpEntry->mbTestConnectionPacketDelivered)
        {
            lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNECTION_SUCCESS;
            if (mpfConnectionFinalisedCallback)
            {
                mpfConnectionFinalisedCallback(true, lPlayerID, lpEntry->mConnectionData,
                                               mpConnectionFinalisedUserData);
            }
        }

        if (lpEntry->mbTestConnectionPacketNacked)
        {
            SendTestConnectionMessage(lpEntry, lpTimerStatus, lu16CurrentFrame);
        }

        if (lpTimerStatus->GetTime() - lpEntry->mTimeTestConnectionSent > K_TEST_PACKET_TIMEOUT_SECONDS)
        {
            lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNECTION_FAILURE;
            if (mpfConnectionFinalisedCallback)
            {
                mpfConnectionFinalisedCallback(false, lPlayerID, lpEntry->mConnectionData,
                                               mpConnectionFinalisedUserData);
            }
        }
    }
}

// ---- SendTestConnectionMessage -------------------------------------------------------
void PlayersConnectionManager::SendTestConnectionMessage(ConnectionDataEntry* lpEntry,
                                                         const CgsSystem::TimerStatus* lpTimerStatus,
                                                         u16 lu16CurrentFrame)
{
    CGS_ASSERT_AT(lpEntry->mPlayerConnectionData.meConnectionStatus == E_CHECKING_CONNECTION,
        "lpEntry->mPlayerConnectionData.meConnectionStatus == E_CHECKING_CONNECTION",
            CGS_ARTIST_FILE, 693 /* ARTIST 0x82884ACC */);


    lpEntry->mbTestConnectionPacketNacked = false;
    lpEntry->mTestConnectionMessageSend.PrepareForSend(lu16CurrentFrame);
    lpEntry->mTimeTestConnectionSent = lpTimerStatus->GetTime();
}

// ---- SendConnectionStatusMessages ----------------------------------------------------
// On its round-robin turn, every connected peer is sent our view of all the entries when
// it differs from the view we last sent it.
void PlayersConnectionManager::SendConnectionStatusMessages(u16 lu16CurrentFrame)
{
    PlayerConnectionData laConnectionData[KI_CONNECTION_STATUS_PLAYER_COUNT];
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        laConnectionData[liIndex] = maConnectionDataEntry[liIndex].mPlayerConnectionData;
    }

    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_CONNECTION_ENTRIES; ++liPlayerIndex)
    {
        ConnectionDataEntry* lpEntry = &maConnectionDataEntry[liPlayerIndex];
        bool lbConnectionStatusIsUpToDate = true;

        if (lpEntry->mPlayerConnectionData.mPlayerID == K_INVALID_PLAYER_ID)
        {
            continue;
        }
        if (!mpPlayerManager->IsPlayerTurnToSendRoundRobinMessage(lpEntry->mPlayerConnectionData.mPlayerID,
                                                                  true, 0))
        {
            continue;
        }
        if (lpEntry->mPlayerConnectionData.meConnectionStatus != E_CONNECTION_SUCCESS)
        {
            continue;
        }

        for (s32 liArrayIndex = 0; liArrayIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liArrayIndex)
        {
            if (lpEntry->maLastFinaliseStateSent[liArrayIndex].mPlayerID != laConnectionData[liArrayIndex].mPlayerID
                || lpEntry->maLastFinaliseStateSent[liArrayIndex].meConnectionStatus
                       != laConnectionData[liArrayIndex].meConnectionStatus)
            {
                lbConnectionStatusIsUpToDate = false;
                break;
            }
        }

        if (!lbConnectionStatusIsUpToDate)
        {
            lpEntry->mConnectionStatusMessageSend.PrepareForSend(laConnectionData, lu16CurrentFrame);
            for (s32 liArrayIndex = 0; liArrayIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liArrayIndex)
            {
                lpEntry->maLastFinaliseStateSent[liArrayIndex] = laConnectionData[liArrayIndex];
            }
        }
    }
}

// ---- TestConnectionMessageDeliveredCallback ------------------------------------------
// An ack marks the test message delivered; a real nack (not a faked one) asks for a resend.
void PlayersConnectionManager::TestConnectionMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                                      SignalMessage* lpAck,
                                                                      NetworkPlayerID lRecvingPlayerID,
                                                                      void* lpUserData)
{
    PlayersConnectionManager* lpConnMgr = static_cast<PlayersConnectionManager*>(lpUserData);
    ConnectionDataEntry*      lpEntry   = lpConnMgr->GetEntry(lRecvingPlayerID);
    if (lpEntry == 0)
    {
        return;
    }

    if (lbSuccess)
    {
        lpEntry->mbTestConnectionPacketDelivered = true;
    }
    else if (!lbFakeNack)
    {
        lpEntry->mbTestConnectionPacketNacked = true;
    }
}

// ---- TestConnectionMessageArrivedCallback --------------------------------------------
void PlayersConnectionManager::TestConnectionMessageArrivedCallback(ReliableMessage* lpMessage,
                                                                    NetworkPlayerID lSendingPlayerID,
                                                                    void* lpUserData)
{
    PlayersConnectionManager* lpConnMgr = static_cast<PlayersConnectionManager*>(lpUserData);
    ConnectionDataEntry*      lpEntry   = lpConnMgr->GetEntry(lSendingPlayerID);
    if (lpEntry)
    {
        lpEntry->mbHaveReceivedTestPacketFromPlayer = true;
    }
}

// ---- ConnectionStatusMessageDeliveredCallback ----------------------------------------
// A lost status message forgets what we last sent that peer, so the next turn resends it.
void PlayersConnectionManager::ConnectionStatusMessageDeliveredCallback(bool lbSuccess, bool lbFakeNack,
                                                                        SignalMessage* lpAck,
                                                                        NetworkPlayerID lRecvingPlayerID,
                                                                        void* lpUserData)
{
    if (lbSuccess)
    {
        return;
    }

    PlayersConnectionManager* lpConnMgr = static_cast<PlayersConnectionManager*>(lpUserData);
    ConnectionDataEntry*      lpEntry   = lpConnMgr->GetEntry(lRecvingPlayerID);
    if (lpEntry == 0)
    {
        return;
    }

    for (s32 liIndex = 0; liIndex < KI_CONNECTION_STATUS_PLAYER_COUNT; ++liIndex)
    {
        lpEntry->maLastFinaliseStateSent[liIndex].mPlayerID          = K_INVALID_PLAYER_ID;
        lpEntry->maLastFinaliseStateSent[liIndex].meConnectionStatus = E_NOT_STARTED;
    }
}

// ---- ConnectionStatusMessageArrivedCallback ------------------------------------------
// Store the sender's view of every connection as its last received finalise state.
void PlayersConnectionManager::ConnectionStatusMessageArrivedCallback(ReliableMessage* lpMessage,
                                                                      NetworkPlayerID lSendingPlayerID,
                                                                      void* lpUserData)
{
    CGS_ASSERT_AT(lpMessage,
        "lpMessage", CGS_ARTIST_FILE, 1305 /* ARTIST 0x82884E00 */);
    CGS_ASSERT_AT(lSendingPlayerID != K_INVALID_PLAYER_ID,
        "lSendingPlayerID != K_INVALID_PLAYER_ID",
            CGS_ARTIST_FILE, 1306 /* ARTIST 0x82884E24 */);

    CGS_ASSERT_AT(lpUserData,
        "lpUserData", CGS_ARTIST_FILE, 1307 /* ARTIST 0x82884E48 */);

    PlayersConnectionManager* lpConnMgr = static_cast<PlayersConnectionManager*>(lpUserData);
    ConnectionDataEntry*      lpEntry   = lpConnMgr->GetEntry(lSendingPlayerID);
    if (lpEntry)
    {
        ConnectionStatusMessage* lpConnStatMsg = static_cast<ConnectionStatusMessage*>(lpMessage);
        lpConnStatMsg->Retrieve(lpEntry->maLastFinaliseStateRecvd);
    }
}

}

// FOLDED FROM CgsPlayersConnectionManager_wN2_03.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // registry lookups / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // SetConnectionData / GetName
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "connapi.h"     // ConnApiGetClientList, ConnApiClientListT
#include "lobbyname.h"   // LobbyNameCmp

// CgsNetwork::PlayersConnectionManager -- the kick choice and the ConnApi status callback.

namespace CgsNetwork
{

// ---- GetIDOfPlayerToKick -------------------------------------------------------------
// Of two players who cannot reach each other, the one to drop: never the host; otherwise
// whichever of the two appears later in the ConnApi member list, falling back to the higher
// id when the list does not settle it.
NetworkPlayerID PlayersConnectionManager::GetIDOfPlayerToKick(NetworkPlayerID lPlayerID1,
                                                              NetworkPlayerID lPlayerID2) const
{
    CGS_ASSERT_AT(lPlayerID1 != lPlayerID2, "Don't call with the same Player IDs", CGS_ARTIST_FILE, 865);

    const NetworkPlayerID lHostPlayerID = mpPlayerManager->mHostPlayerID;
    if (lPlayerID1 == lHostPlayerID)
    {
        return lPlayerID2;
    }
    if (lPlayerID2 == lHostPlayerID)
    {
        return lPlayerID1;
    }

    const char* lpcName1 = GetPlayerName(lPlayerID1);
    const char* lpcName2 = GetPlayerName(lPlayerID2);

    DirtySock::ConnApiRefT* lpConnApi = mpServerInterface->GetConnAPIRef();
    if (lpConnApi != 0)
    {
        const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
        if (lpClientList != 0 && lpcName1 != 0 && lpcName2 != 0)
        {
            for (s32 liIndex = lpClientList->iNumClients - 1; liIndex >= 0; --liIndex)
            {
                const char* lpcClientName = lpClientList->Clients[liIndex].UserInfo.strName;
                if (LobbyNameCmp(lpcName1, lpcClientName) == 0)
                {
                    return lPlayerID1;
                }
                if (LobbyNameCmp(lpcName2, lpcClientName) == 0)
                {
                    return lPlayerID2;
                }
            }
        }
    }

    if (lPlayerID1 > lPlayerID2)
    {
        return lPlayerID1;
    }
    return lPlayerID2;
}

// ---- ConnApiStatusChangeCallback -----------------------------------------------------
// A member's game connection went away: clear its player's connection block and report the
// player disconnected. (User data = the manager.)
void PlayersConnectionManager::ConnApiStatusChangeCallback(DirtySock::ConnApiRefT* lpConnApi,
                                                           DirtySock::ConnApiCbInfoT* lpCbInfo,
                                                           void* lpUserData)
{
    PlayersConnectionManager* lpSelf = static_cast<PlayersConnectionManager*>(lpUserData);

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    CGS_ASSERT_AT(lpClientList,
        "lpClientList",
        CGS_ARTIST_FILE, 1509 /* ARTIST 0x82874CAC */);

    const bool lbDisconnected = (lpCbInfo->eNewStatus == DirtySock::CONNAPI_STATUS_DISC) ||
                                (lpCbInfo->eNewStatus == DirtySock::CONNAPI_STATUS_DEST);
    if (!lbDisconnected || lpCbInfo->eType != DirtySock::CONNAPI_CBTYPE_GAMEEVENT)
    {
        return;
    }

    const DirtySock::ConnApiClientT* lpClient = &lpClientList->Clients[lpCbInfo->iClientId];
    NetworkPlayer* lpPlayer = lpSelf->mpPlayerManager->GetPlayerByName(lpClient->UserInfo.strName);
    if (lpPlayer == 0)
    {
        return;
    }

    ConnectionData lConnectionData;
    lConnectionData.Clear();
    lpPlayer->SetConnectionData(lConnectionData);

    if (lpSelf->mpfPlayerDisconnectedCallback != 0)
    {
        lpSelf->mpfPlayerDisconnectedCallback(lpPlayer->GetPlayerID(), lpSelf->mpPlayerDisconnectedCallbackData);
    }
}

}

// FOLDED FROM CgsPlayersConnectionManager_wN2_04.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT at ARTIST locations
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // registry lookups / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // SetConnectionData / GetName
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerInfoData.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceDefaultPlayerParameters.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "connapi.h"     // ConnApiGetClientList, ConnApiClientListT
#include "lobbyapi.h"    // LobbyApiStatus, LobbyApiUserT
#include "lobbyname.h"   // LobbyNameCmp

// CgsNetwork::PlayersConnectionManager -- reconciling the registry with the ConnApi member
// list, and the local-player name lookup.

namespace CgsNetwork
{

// The lobby status selector for the local user's record.
static const s32 KI_LOBBY_STATUS_SELF = 0x73656C66;   // 'self'

// ---- GetPlayerName -------------------------------------------------------------------
// A registered player's name; the local player's comes from the player-info component.
const char* PlayersConnectionManager::GetPlayerName(NetworkPlayerID lPlayerID) const
{
    const NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
    if (lpPlayer != 0)
    {
        return lpPlayer->GetName();
    }

    // The console returned the address of its stack copy of the record; the PC keeps the copy
    // in static storage so the name outlives the call.
    static DefaultPlayerInfoData sLocalPlayerInfo;

    CGS_ASSERT_AT(mpServerInterface,
        "mpServerInterface",
        CGS_ARTIST_FILE, 836 /* ARTIST 0x8289057C */);
    static_cast<ServerInterfacePlayerInfo*>(mpServerInterface->GetComponent(E_COMPONENTS_PLAYER_INFO))
        ->GetLocalPlayerInfo(&sLocalPlayerInfo);
    if (sLocalPlayerInfo.GetID() == lPlayerID)
    {
        return sLocalPlayerInfo.GetName();
    }

    CGS_FAIL_AT(
        "Could not find name of player",
        CGS_ARTIST_FILE, 844 /* ARTIST 0x828905E8 */);
    return 0;
}

// ---- UpdatePlayerList ----------------------------------------------------------------
// Bring the registry in line with the ConnApi member list. In a game: add every member not yet
// known (the local player through the lobby's 'self' record, a remote one with the id its
// lobby parameters carry), then follow each remote member's game connection -- demangling
// marks the handshake started, an active link fills the player's connection block and starts
// the test-connection exchange, a lost one finalises the connection as failed. Out of a game
// only presence is tracked. Every registered player no longer in the list is removed.
void PlayersConnectionManager::UpdatePlayerList(const DirtySock::ConnApiClientListT* lpClientList,
                                                const CgsSystem::TimerStatus* lpTimerStatus,
                                                u16 lu16CurrentFrame)
{
    bool lbLocalPlayerInList = false;
    bool labEntryInList[KI_MAX_CONNECTION_ENTRIES];
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        labEntryInList[liIndex] = false;
    }

    DefaultPlayerInfoData lLocalPlayerInfo;
    LobbyApiUserT         lSelfUser;
    LobbyApiStatus(mpServerInterface->GetLobbyAPIRef(), KI_LOBBY_STATUS_SELF, &lSelfUser,
                   static_cast<s32>(sizeof(lSelfUser)));
    lLocalPlayerInfo.SerialiseFromUser(&lSelfUser);

    ServerInterfaceGames* lpGames = static_cast<ServerInterfaceGames*>(mpServerInterface->GetGameComponent());

    if (lpGames->IsLocalPlayerInGame())
    {
        for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
        {
            const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];
            const bool lbLocal = (LobbyNameCmp(lLocalPlayerInfo.GetName(), lClient.UserInfo.strName) == 0);

            if (lbLocal)
            {
                lbLocalPlayerInList = true;
                const NetworkPlayerID lSelfID = lLocalPlayerInfo.GetID();
                const NetworkPlayerID lPMLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
                if (lPMLocalPlayerID != lSelfID)
                {
                    CGS_ASSERT(lPMLocalPlayerID == K_INVALID_PLAYER_ID, "lPMLocalPlayerID == K_INVALID_PLAYER_ID");
                    AddPlayer(lpTimerStatus, lClient.UserInfo.strName, lSelfID, liClient, lbLocal);
                    muLocalPlayerIPAddress = lClient.UserInfo.uAddr;
                }
                continue;
            }

            ConnectionDataEntry* lpEntry = 0;
            NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByName(lClient.UserInfo.strName);
            if (lpPlayer != 0)
            {
                lpEntry = GetEntry(lpPlayer->GetPlayerID());
                CGS_ASSERT(lpEntry, "lpEntry");
            }
            else
            {
                if (lClient.GameInfo.eStatus == DirtySock::CONNAPI_STATUS_DISC ||
                    lClient.GameInfo.eStatus == DirtySock::CONNAPI_STATUS_DEST)
                {
                    continue;
                }

                const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
                DefaultPlayerParameters lPlayerParams;
                lPlayerParams.Prepare();
                lpGames->GetPlayerParametersByPlayerName(lClient.UserInfo.strName, &lPlayerParams);
                const NetworkPlayerID lNewPlayerID = lPlayerParams.GetID();
                CGS_ASSERT(lNewPlayerID != lLocalPlayerID, "lNewPlayerID != lLocalPlayerID");
                CGS_ASSERT(lNewPlayerID != K_INVALID_PLAYER_ID, "lNewPlayerID != K_INVALID_PLAYER_ID");

                lpEntry = AddPlayer(lpTimerStatus, lClient.UserInfo.strName, lNewPlayerID, liClient, lbLocal);
                CGS_ASSERT(lpEntry, "lpEntry");
            }

            const s32 liEntryOffset = static_cast<s32>(lpEntry - maConnectionDataEntry);
            CGS_ASSERT(liEntryOffset >= 0, "liEntryOffset >= 0");
            CGS_ASSERT(liEntryOffset < KI_MAX_CONNECTION_ENTRIES, "liEntryOffset < KI_MAX_NETWORK_PLAYERS");
            labEntryInList[liEntryOffset] = true;

            switch (lClient.GameInfo.eStatus)
            {
                case DirtySock::CONNAPI_STATUS_INIT:
                case DirtySock::CONNAPI_STATUS_CONN:
                case DirtySock::CONNAPI_STATUS_CLSE:
                    break;

                case DirtySock::CONNAPI_STATUS_MNGL:
                    lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNAPI_PROTOMANGLING;
                    break;

                case DirtySock::CONNAPI_STATUS_ACTV:
                    if (lpEntry->mPlayerConnectionData.meConnectionStatus == E_NOT_STARTED ||
                        lpEntry->mPlayerConnectionData.meConnectionStatus == E_CONNAPI_PROTOMANGLING)
                    {
                        lpEntry->mPlayerConnectionData.meConnectionStatus = E_CHECKING_CONNECTION;

                        ConnectionData lConnectionData;
                        lConnectionData.Clear();
                        lConnectionData.mpNetGameLink         = lClient.pGameLinkRef;
                        lConnectionData.miIPAddress           = static_cast<s32>(lClient.UserInfo.uAddr);
                        lConnectionData.miLocalIPAddress      = static_cast<s32>(lClient.UserInfo.uLocalAddr);
                        lConnectionData.muGamePort            = lClient.UserInfo.uLocalGamePort;
                        lConnectionData.muVoipPort            = lClient.UserInfo.uLocalVoipPort;
                        lConnectionData.muLocalGamePort       = lClient.GameInfo.uLocalPort;
                        lConnectionData.muMnglGamePort        = lClient.GameInfo.uMnglPort;
                        lConnectionData.muLocalVoipPort       = lClient.VoipInfo.uLocalPort;
                        lConnectionData.muMnglVoipPort        = lClient.VoipInfo.uMnglPort;
                        lConnectionData.muConnApiClientId     = lClient.UserInfo.uClientId;
                        lConnectionData.meGameConnectionType  = (lClient.GameInfo.uConnFlags >> 1) & 1;
                        lConnectionData.meVoipConnectionType  = (lClient.VoipInfo.uConnFlags >> 1) & 1;
                        lpEntry->mConnectionData = lConnectionData;

                        NetworkPlayer* lpConnectedPlayer =
                            mpPlayerManager->GetPlayerByID(lpEntry->mPlayerConnectionData.mPlayerID);
                        lpConnectedPlayer->SetConnectionData(lConnectionData);
                        lpConnectedPlayer->SendDirtySockConnectionTelemetry(lClient.GameInfo.uConnFlags,
                                                                            lClient.VoipInfo.uConnFlags);
                        SendTestConnectionMessage(lpEntry, lpTimerStatus, lu16CurrentFrame);
                    }
                    break;

                case DirtySock::CONNAPI_STATUS_DISC:
                case DirtySock::CONNAPI_STATUS_DEST:
                    if (lpEntry->mPlayerConnectionData.meConnectionStatus == E_NOT_STARTED ||
                        lpEntry->mPlayerConnectionData.meConnectionStatus == E_CONNAPI_PROTOMANGLING)
                    {
                        lpEntry->mPlayerConnectionData.meConnectionStatus = E_CONNECTION_FAILURE;
                        if (mpfConnectionFinalisedCallback != 0)
                        {
                            mpfConnectionFinalisedCallback(false, lpEntry->mPlayerConnectionData.mPlayerID,
                                                           lpEntry->mConnectionData,
                                                           mpConnectionFinalisedUserData);
                        }
                    }
                    break;

                default:
                    CGS_FAIL("Unknown connapi status");
                    break;
            }
        }
    }
    else
    {
        for (s32 liClient = 0; liClient < lpClientList->iNumClients; ++liClient)
        {
            const DirtySock::ConnApiClientT& lClient = lpClientList->Clients[liClient];
            if (LobbyNameCmp(lLocalPlayerInfo.GetName(), lClient.UserInfo.strName) == 0)
            {
                lbLocalPlayerInList = true;
                continue;
            }

            NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByName(lClient.UserInfo.strName);
            if (lpPlayer == 0)
            {
                continue;
            }

            ConnectionDataEntry* lpEntry = GetEntry(lpPlayer->GetPlayerID());
            CGS_ASSERT(lpEntry, "lpEntry");
            const s32 liEntryOffset = static_cast<s32>(lpEntry - maConnectionDataEntry);
            CGS_ASSERT(liEntryOffset >= 0, "liEntryOffset >= 0");
            CGS_ASSERT(liEntryOffset < KI_MAX_CONNECTION_ENTRIES, "liEntryOffset < KI_MAX_NETWORK_PLAYERS");
            labEntryInList[liEntryOffset] = true;
        }
    }

    const NetworkPlayerID lLocalPlayerID = mpPlayerManager->GetLocalPlayerID();
    if (!lbLocalPlayerInList && lLocalPlayerID != K_INVALID_PLAYER_ID)
    {
        RemovePlayer(lLocalPlayerID);
    }

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        const NetworkPlayerID lEntryPlayerID = maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID;
        if (labEntryInList[liIndex] || lEntryPlayerID == K_INVALID_PLAYER_ID)
        {
            continue;
        }

        if (lEntryPlayerID == mpPlayerManager->GetLocalPlayerID())
        {
            mpPlayerManager->RemovePlayer(lEntryPlayerID);
            muLocalPlayerIPAddress = 0;
        }
        else
        {
            ConnectionDataEntry* lpEntry = GetEntry(lEntryPlayerID);
            CGS_ASSERT(lpEntry, "lpEntry");
            UnRegisterMessageTypes(lpEntry);
            mpPlayerManager->RemovePlayer(lEntryPlayerID);
            lpEntry->mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
        }
    }
}

}

// FOLDED FROM CgsPlayersConnectionManager_wN2_05.cpp
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"   // AddConnectionStatusChangeCallback

// CgsNetwork::PlayersConnectionManager -- binding the manager to its registry and server
// interface, and hooking the ConnApi status changes.

namespace CgsNetwork
{

// ---- Prepare -------------------------------------------------------------------------
// Latch the registry, the server interface and both callbacks, free every connection entry
// and take the server interface's connection-status callback.
bool PlayersConnectionManager::Prepare(PlayerManager* lpPlayerManager, ServerInterface* lpServerInterface,
                                       ConnMgrConnectionFinalisedCallback lpfConnectionFinalisedCallback,
                                       void* lpConnectionFinalisedUserData,
                                       ConnMgrPlayerDisconnectedCallback lpfPlayerDisconnectedCallback,
                                       void* lpPlayerDisconnectedCallbackData)
{
    mpPlayerManager                  = lpPlayerManager;
    mpfPlayerDisconnectedCallback    = lpfPlayerDisconnectedCallback;
    mpPlayerDisconnectedCallbackData = lpPlayerDisconnectedCallbackData;
    mpfConnectionFinalisedCallback   = lpfConnectionFinalisedCallback;
    mpConnectionFinalisedUserData    = lpConnectionFinalisedUserData;
    mpServerInterface                = lpServerInterface;
    muLocalPlayerIPAddress           = 0;

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
    }

    mpServerInterface->AddConnectionStatusChangeCallback(&ConnApiStatusChangeCallback, this);
    return true;
}

// ---- OnLobbyApiCreated ---------------------------------------------------------------
// A new lobby (and ConnApi) came up: take the connection-status callback again.
void PlayersConnectionManager::OnLobbyApiCreated()
{
    mpServerInterface->AddConnectionStatusChangeCallback(&ConnApiStatusChangeCallback, this);
}

}
