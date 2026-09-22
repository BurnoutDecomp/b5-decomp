#pragma once

// ===================================================================================
// CgsNetwork::PlayersConnectionManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h
//
// The per-peer connection-test state machine the player registry embeds at its +0x0000
// (PlayerManager::mConnectionManager). It tracks one ConnectionDataEntry per remote slot,
// swaps test-connection and connection-status reliable messages with every peer, and
// reports the finalised / disconnected edges back to the registry through two callbacks.
//
// Console layout (0xE38 bytes; Prepare stores every pointer, the ctor seeds the
// embedded message and time members of the seven entries at stride 0x204):
//   +0x000  mpPlayerManager
//   +0x004  mpfConnectionFinalisedCallback
//   +0x008  mpConnectionFinalisedUserData
//   +0x00C  mpfPlayerDisconnectedCallback
//   +0x010  mpPlayerDisconnectedCallbackData
//   +0x014  maConnectionDataEntry[7]            (0x204 each)
//   +0xE30  mpServerInterface
//   +0xE34  muLocalPlayerIPAddress
//
// ConnectionDataEntry (0x204 bytes):
//   +0x000  mPlayerConnectionData               (the entry's player id is its first word)
//   +0x008  maLastFinaliseStateSent[7]
//   +0x040  maLastFinaliseStateRecvd[7]
//   +0x078  mConnectionData                     (0x70)
//   +0x0E8  mTestConnectionMessageSend          (0x28)
//   +0x110  mTestConnectionMessageRecv          (0x28)
//   +0x138  mTimeTestConnectionSent
//   +0x140  mbHaveReceivedTestPacketFromPlayer
//   +0x141  mbTestConnectionPacketDelivered
//   +0x142  mbTestConnectionPacketNacked
//   +0x144  mConnectionStatusMessageSend        (0x60)
//   +0x1A4  mConnectionStatusMessageRecv        (0x60)
//
// Members are reached by name; the offsets above are console facts (the host widens
// every pointer).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Network/Packeting/CgsNetworkAdapterBase.h"              // ConnectionData
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsTestConnectionMessage.h"
#include "GameShared/GameClasses/Network/Players/CgsConnectionStatusMessage.h"          // PlayerConnectionData, EConnectionStatus

namespace CgsSystem
{
    class TimerStatus;
}

namespace CgsNetwork
{
    struct PlayerManager;
    struct SignalMessage;
    struct ReliableMessage;
    class  ServerInterface;

    namespace DirtySock
    {
        struct ConnApiRefT;
        struct ConnApiCbInfoT;
        struct ConnApiClientListT;
    }

    typedef s32 NetworkPlayerID;

    class PlayersConnectionManager
    {
    public:
        static const s32 KI_MAX_CONNECTION_ENTRIES = 7;

        typedef void (*ConnMgrConnectionFinalisedCallback)(bool lbSuccess,
                                                            NetworkPlayerID lPlayerID,
                                                            ConnectionData lConnectionData,
                                                            void* lpUserData);
        typedef void (*ConnMgrPlayerDisconnectedCallback)(NetworkPlayerID lPlayerID,
                                                          void* lpUserData);

        struct ConnectionDataEntry
        {
            PlayerConnectionData    mPlayerConnectionData;                                   // +0x000
            PlayerConnectionData    maLastFinaliseStateSent[KI_CONNECTION_STATUS_PLAYER_COUNT];  // +0x008
            PlayerConnectionData    maLastFinaliseStateRecvd[KI_CONNECTION_STATUS_PLAYER_COUNT]; // +0x040
            ConnectionData          mConnectionData;                                         // +0x078
            TestConnectionMessage   mTestConnectionMessageSend;                              // +0x0E8
            TestConnectionMessage   mTestConnectionMessageRecv;                              // +0x110
            CgsSystem::Time         mTimeTestConnectionSent;                                 // +0x138
            bool                    mbHaveReceivedTestPacketFromPlayer;                      // +0x140
            bool                    mbTestConnectionPacketDelivered;                         // +0x141
            bool                    mbTestConnectionPacketNacked;                            // +0x142
            ConnectionStatusMessage mConnectionStatusMessageSend;                            // +0x144
            ConnectionStatusMessage mConnectionStatusMessageRecv;                            // +0x1A4
        };

        PlayersConnectionManager();

        void Construct();
        void Destruct();
        bool Prepare(PlayerManager* lpPlayerManager, ServerInterface* lpServerInterface,
                     ConnMgrConnectionFinalisedCallback lpfConnectionFinalisedCallback,
                     void* lpConnectionFinalisedUserData,
                     ConnMgrPlayerDisconnectedCallback lpfPlayerDisconnectedCallback,
                     void* lpPlayerDisconnectedCallbackData);
        bool Release();
        void Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);

        EConnectionStatus GetConnectionStatus(NetworkPlayerID lPlayerID) const;
        void ResetConnectionData();
        bool HavePlayersFailedToConnect(NetworkPlayerID lPlayerID1, NetworkPlayerID lPlayerID2) const;
        NetworkPlayerID GetIDOfPlayerToKick(NetworkPlayerID lPlayerID1, NetworkPlayerID lPlayerID2) const;
        bool AreAllConnectionsSuccessful() const;
        void Disconnected();
        void OnLobbyApiCreated();

    private:
        const char* GetPlayerName(NetworkPlayerID lPlayerID) const;
        ConnectionDataEntry* AddPlayer(const CgsSystem::TimerStatus* lpTimerStatus,
                                       const char* lpcName, NetworkPlayerID lPlayerID,
                                       s32 liConnectionIndex, bool lbLocal);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        const ConnectionDataEntry* GetEntry(NetworkPlayerID lPlayerID) const;
        ConnectionDataEntry*       GetEntry(NetworkPlayerID lPlayerID);
        void RegisterMessageTypes(ConnectionDataEntry* lpEntry);
        void UnRegisterMessageTypes(ConnectionDataEntry* lpEntry);
        void CheckAndUpdateGameID();
        void UpdatePlayerList(const DirtySock::ConnApiClientListT* lpClientList,
                              const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame);
        void CheckTestConnectionMessageStatus(const CgsSystem::TimerStatus* lpTimerStatus,
                                              u16 lu16CurrentFrame);
        void SendTestConnectionMessage(ConnectionDataEntry* lpEntry,
                                       const CgsSystem::TimerStatus* lpTimerStatus,
                                       u16 lu16CurrentFrame);
        void SendConnectionStatusMessages(u16 lu16CurrentFrame);

        // Message and ConnAPI callbacks, registered by address (user data = the manager).
        static void TestConnectionMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                           SignalMessage* lpMessage,
                                                           NetworkPlayerID lPlayerID,
                                                           void* lpUserData);
        static void TestConnectionMessageArrivedCallback(ReliableMessage* lpMessage,
                                                         NetworkPlayerID lSendingPlayerID,
                                                         void* lpUserData);
        static void ConnectionStatusMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                             SignalMessage* lpMessage,
                                                             NetworkPlayerID lPlayerID,
                                                             void* lpUserData);
        static void ConnectionStatusMessageArrivedCallback(ReliableMessage* lpMessage,
                                                           NetworkPlayerID lSendingPlayerID,
                                                           void* lpUserData);
        static void ConnApiStatusChangeCallback(DirtySock::ConnApiRefT* lpConnApi,
                                                DirtySock::ConnApiCbInfoT* lpCbInfo,
                                                void* lpUserData);

        PlayerManager*                     mpPlayerManager;                                  // +0x000
        ConnMgrConnectionFinalisedCallback mpfConnectionFinalisedCallback;                   // +0x004
        void*                              mpConnectionFinalisedUserData;                    // +0x008
        ConnMgrPlayerDisconnectedCallback  mpfPlayerDisconnectedCallback;                    // +0x00C
        void*                              mpPlayerDisconnectedCallbackData;                 // +0x010
        ConnectionDataEntry                maConnectionDataEntry[KI_MAX_CONNECTION_ENTRIES]; // +0x014
        ServerInterface*                   mpServerInterface;                                // +0xE30
        u32                                muLocalPlayerIPAddress;                           // +0xE34
    };
}
