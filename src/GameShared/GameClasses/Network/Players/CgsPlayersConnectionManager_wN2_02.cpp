#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                  // GetPlayerByID / AddPlayer / RemovePlayer
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                  // RegisterMessageType / UnRegisterMessageType
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterface.h"        // GetGameComponent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"              // TimerStatus::GetTime
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::PlayersConnectionManager -- the connection-entry bookkeeping (add / remove a
// peer, register its two message types), the test-connection handshake, the connection
// status broadcast and the message callbacks the network players invoke.

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
    ConnectionDataEntry* lpEntry = nullptr;

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
    NetworkPlayerID lLocalPlayerID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    if (lPlayerID == lLocalPlayerID)
    {
        mpPlayerManager->RemovePlayer(lPlayerID);
        muLocalPlayerIPAddress = 0;
        return;
    }

    ConnectionDataEntry* lpEntry = GetEntry(lPlayerID);
    CGS_ASSERT(lpEntry, "lpEntry");

    UnRegisterMessageTypes(lpEntry);
    mpPlayerManager->RemovePlayer(lPlayerID);
    lpEntry->mPlayerConnectionData.mPlayerID = K_INVALID_PLAYER_ID;
}

// ---- RegisterMessageTypes ------------------------------------------------------------
// The entry's send / receive message pairs go onto the peer's network player, with this
// manager as the callbacks' user data.
void PlayersConnectionManager::RegisterMessageTypes(ConnectionDataEntry* lpEntry)
{
    NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lpEntry->mPlayerConnectionData.mPlayerID);
    CGS_ASSERT(lpNetPlayer, "lpNetPlayer");

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
    NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lpEntry->mPlayerConnectionData.mPlayerID);
    CGS_ASSERT(lpNetPlayer, "lpNetPlayer");

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
    CGS_ASSERT(lpEntry->mPlayerConnectionData.meConnectionStatus == E_CHECKING_CONNECTION,
               "lpEntry->mPlayerConnectionData.meConnectionStatus == E_CHECKING_CONNECTION");

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
    if (lpEntry == nullptr)
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
    if (lpEntry == nullptr)
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
    CGS_ASSERT(lpMessage, "lpMessage");
    CGS_ASSERT(lSendingPlayerID != K_INVALID_PLAYER_ID, "lSendingPlayerID != K_INVALID_PLAYER_ID");
    CGS_ASSERT(lpUserData, "lpUserData");

    PlayersConnectionManager* lpConnMgr = static_cast<PlayersConnectionManager*>(lpUserData);
    ConnectionDataEntry*      lpEntry   = lpConnMgr->GetEntry(lSendingPlayerID);
    if (lpEntry)
    {
        ConnectionStatusMessage* lpConnStatMsg = static_cast<ConnectionStatusMessage*>(lpMessage);
        lpConnStatMsg->Retrieve(lpEntry->maLastFinaliseStateRecvd);
    }
}

}
