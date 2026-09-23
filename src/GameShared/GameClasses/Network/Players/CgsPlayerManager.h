#pragma once

// ===================================================================================
// CgsNetwork::PlayerManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/Players/CgsPlayerManager.h
//
// The session-wide player registry (embedded in CgsNetwork::NetworkManager at +0x78).
// It owns the connection-test manager and the reliable-message queue, the per-slot
// ack/nack signal messages, the active/inactive player tables, the event callbacks and
// the per-frame send/receive pumps.
//
// Console layout (0x2500 bytes, 8-aligned by the reliable-message bit array). Every
// offset is the one Construct / Prepare / Release and the lookups address:
//   +0x0000  mConnectionManager                 (PlayersConnectionManager, 0xE38)
//   +0x0E38  mReliableMessageManager            (ReliableMessageManager, 0x11B8)
//   +0x1FF0  maAckMessage[10]                   (SignalMessage, 0x28 each)
//   +0x2180  maNackMessage[10]
//   +0x2310  maInactivePlayers[8]               (PlayerData, 12 each)
//   +0x2370  maActivePlayers[8]
//   +0x23D0  maEventCallbacks[1]
//   +0x23D8  miNumEventCallbacks
//   +0x23DC  mePrepareState
//   +0x23E0  miNumActivePlayers
//   +0x23E4  miNumInactivePlayers
//   +0x23E8  mpServerInterface
//   +0x23EC  mpNetworkAdapter
//   +0x23F0  mpTimeManager
//   +0x23F4  meLocalConsoleFrameRate
//   +0x23F8  mHostPlayerID
//   +0x23FC  mLocalPlayerID
//   +0x2400  mpfOnReceivedFromWrongIPCallback
//   +0x2404  mpfConnectionFinalisedCallback
//   +0x2408  mpConnectionFinalisedUserData
//   +0x240C  mu16CurrentFrame
//   +0x240E  mu8GameID
//   +0x240F  mbDiskAccessible
//   +0x2410  mbPlayerListIsValid
//   +0x2414  miPLAYERManagerSendMessagesPM
//   +0x2418  mDebugComponent                    (PlayerManagerDebugComponent, 0x24)
//   +0x243C  miBytesUsedForAcks
//   +0x2440  miBytesUsedForUnreliableMessages
//   +0x2444  miBytesUsedForReliableMessages
//   +0x2448  miBytesUsedForReliableResendMessages
//   +0x244C  maMessageBytes[44]                 (Construct clears 44 words)
//
// Members are reached by name; the offsets above are console facts (the host widens
// every pointer).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"                         // CgsSystem::EFrameRate
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                         // K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Network/Players/CgsReliableMessageManager.h"
#include "GameShared/GameClasses/Network/Players/CgsConnectionStatusMessage.h"          // EConnectionStatus
#include "GameShared/GameClasses/Network/Players/CgsPlayerDescriptionsArray.h"          // PlayerData
#include "GameShared/GameClasses/Network/Packeting/Messages/CgsSignalMessage.h"
#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.h"

namespace CgsSystem
{
    class TimerStatus;
}

namespace CgsMemory
{
    class HeapMalloc;
}

namespace CgsNetwork
{
    struct Message;
    struct NetworkPlayer;
    struct NetworkAdapter;
    struct PlayerMenuData;
    struct TimeManager;
    struct CgsNetworkPlayerConstructParams;
    class  ServerInterface;
    class  PlayersConnectionManager;

    // Tables handed to PlayerManager::Construct: per remote slot the player's construct
    // params, the NetworkPlayer object and its menu-data object.
    struct PlayerManagerConstructParams
    {
        CgsNetworkPlayerConstructParams* mapConstructParams[8];   // +0x00
        NetworkPlayer*                   mapPlayerList[8];        // +0x20
        PlayerMenuData*                  mapMenuData[8];          // +0x40
    };

    // The block PlayerManager::Prepare reads (0x20 console bytes).
    struct PlayerManagerPrepareParams
    {
        typedef void OnReceivedFromWrongIPCallback(NetworkPlayer* lpPlayer, s32 liExpectedIP,
                                                   s32 liReceivedIP);

        NetworkAdapter*                 mpNetworkAdapter;                    // +0x00
        ServerInterface*                mpServerInterface;                   // +0x04
        TimeManager*                    mpTimeManager;                       // +0x08
        CgsSystem::EFrameRate           meLocalConsoleFrameRate;             // +0x0C
        OnReceivedFromWrongIPCallback*  mpfOnReceivedFromWrongIPCallback;    // +0x10
        PlayersConnectionManager::ConnMgrConnectionFinalisedCallback
                                        mpfConnectionFinalisedCallback;      // +0x14
        void*                           mpConnectionFinalisedUserData;       // +0x18
        CgsMemory::HeapMalloc*          mpNetworkHeapAllocator;              // +0x1C
    };

    struct PlayerManager
    {
        static const s32 KI_MAX_ACKS_TO_BUFFER  = 10;
        static const s32 KI_MAX_NACKS_TO_BUFFER = 10;
        static const s32 KI_MAX_PLAYERS         = 8;
        static const s32 KI_NUM_EVENT_CALLBACKS = 1;
        static const s32 KI_NUM_MESSAGE_BYTE_COUNTERS = 44;

        // Registry events broadcast to the registered callbacks.
        enum EEvent
        {
            E_EVENT_PLAYER_ADDED            = 0,
            E_EVENT_START_PLAYER_REMOVAL    = 1,
            E_EVENT_END_PLAYER_REMOVAL      = 2,
            E_EVENT_PLAYER_FINALISED        = 3,
            E_EVENT_PLAYER_LOST_CONTACT     = 4,
            E_EVENT_PLAYER_REGAINED_CONTACT = 5,
            E_EVENT_PLAYER_DISCONNECTED     = 6,
            E_EVENT_COUNT                   = 7,
        };

        // Which players a registry walk should visit.
        enum EPlayersToConsider
        {
            E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED = 0,
            E_CONSIDER_ALL_PLAYERS                = 1,
            E_CONSIDER_PLAYERS_COUNT              = 2,
        };

        enum EPrepareState
        {
            E_CONSTRUCTED                        = 0,
            E_RELEASED                           = 1,
            E_PREPARING_CONNECTION_MANAGER       = 2,
            E_PREPARING_RELIABLE_MESSAGE_MANAGER = 3,
            E_FULLY_PREPARED                     = 4,
            E_PREPARING_COUNT                    = 5,
        };

        typedef void (*EventCallbackFunction)(EEvent leEvent, void* lpEventData, void* lpUserData);

        struct EventCallback
        {
            EventCallbackFunction mCallback;     // +0x00
            void*                 mpUserData;    // +0x04
        };

        PlayerManager();

        // --- lifecycle ---
        void Construct(PlayerManagerConstructParams* lpConstructParams);
        void Destruct();
        bool Prepare(PlayerManagerPrepareParams* lpPrepareParams);
        bool Release();
        void Update(const CgsSystem::TimerStatus* lpTimerStatus, u16 lu16CurrentFrame, bool lbInGame);
        void PostUpdate(u16 lu16CurrentFrame);

        // --- players ---
        void AddPlayer(const CgsSystem::TimerStatus* lpTimerStatus, const char* lpcName,
                       NetworkPlayerID lPlayerID, s32 liConnectionIndex,
                       CgsSystem::EFrameRate leRemoteConsoleFrameRate, bool lbLocal);
        void RemovePlayer(NetworkPlayerID lPlayerID);

        // Iterate player ids: seed *lpPlayerID with -1, then call repeatedly; false when the
        // walk is exhausted.
        bool GetNextPlayerID(NetworkPlayerID* lpPlayerID, EPlayersToConsider leConsider) const;
        // Header-inline on the console: every caller reads +0x23FC and tests it against the
        // invalid id.
        bool GetNextLocalPlayerID(NetworkPlayerID* lpPlayerID) const
        {
            *lpPlayerID = mLocalPlayerID;
            return mLocalPlayerID != K_INVALID_PLAYER_ID;
        }
        bool GetNextRemotePlayerID(NetworkPlayerID* lpPlayerID) const;

        NetworkPlayer*  GetPlayerByID(NetworkPlayerID lPlayerID) const;
        NetworkPlayer*  GetPlayerByName(const char* lpcName);
        PlayerMenuData* GetMenuDataByID(NetworkPlayerID lPlayerID) const;

        // --- message pumps ---
        void SendMessages();
        void ReceiveMessages();

        bool AckNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const;
        bool NackNeedsSending(NetworkPlayerID lPlayerID, s32 liIndex) const;
        // Inlined on the console with two range asserts; declared-only until those strings
        // are recovered.
        SignalMessage* GetAck(s32 liIndex);
        SignalMessage* GetNack(s32 liIndex);

        // --- host / identity ---
        NetworkPlayerID GetHostPlayerID();
        void            SetHostPlayerID(NetworkPlayerID lPlayerID);
        bool            AmIHost();
        NetworkPlayerID GetLocalPlayerID() const { return mLocalPlayerID; }
        bool            IsLocalPlayer(NetworkPlayerID lPlayerID) const
        {
            return mLocalPlayerID == lPlayerID && lPlayerID != K_INVALID_PLAYER_ID;
        }

        // The disk-read-error flag the network manager raises and clears each frame.
        void SetDiskAccessible(bool lbDiskAccessible) { mbDiskAccessible = lbDiskAccessible; }

        s32  GetTotalNumberPlayers(EPlayersToConsider leConsider) const;
        s32  GetNumberNetworkPlayers(EPlayersToConsider leConsider) const;
        s32  GetNumberLocalPlayers() const;

        void SetGameID(s32 liGameID);
        u8   GetGameID() const { return mu8GameID; }

        bool IsPlayerFinalised(NetworkPlayerID lPlayerID) const;
        bool IsPlayerTurnToSendRoundRobinMessage(NetworkPlayerID lPlayerID, bool lbInGame,
                                                 s32 liFrameOffset);

        // --- round / session edges ---
        void OnRoundStart();
        void OnRoundFinish();
        void OnLobbyApiCreated();
        void Disconnected();

        void RegisterEventCallback(EventCallbackFunction lpfCallback, void* lpUserData);
        void UnRegisterEventCallback(EventCallbackFunction lpfCallback);

        // Receive-side filters: consume an incoming ack / nack signal, accept or drop a
        // received message.
        bool CheckForAck(Message* lpMessage);
        bool CheckForNack(Message* lpMessage);
        void AcceptMessage(Message* lpMessage);
        void ThrowAwayMessage(Message* lpMessage);

        // --- bandwidth accounting (the debug HUD reads these) ---
        u32 GetTotalBytesSent();
        u32 GetTotalBytesSentWithOverhead();
        u32 GetTotalBytesSentToDirtySock();

        // The connection-test manager embedded at +0x0000. Declared-only: the console has
        // no such accessor (its callers reach mConnectionManager directly).
        PlayersConnectionManager* GetPlayersConnectionManager();

        PlayersConnectionManager mConnectionManager;                         // +0x0000
        ReliableMessageManager   mReliableMessageManager;                    // +0x0E38

    private:
        PlayerData* AssignActiveLocalPlayer(s32 liConnectionIndex);
        PlayerData* AssignActiveNetworkPlayer(s32 liConnectionIndex);
        void        ReleasePlayer(s32 liActiveIndex);
        void        BroadcastEvent(EEvent leEvent, void* lpEventData);

        // Callbacks registered with the connection manager (user data = the registry).
        static void ConnectionFinalisedCallback(bool lbSuccess, NetworkPlayerID lPlayerID,
                                                ConnectionData lConnectionData, void* lpUserData);
        static void PlayerDisconnectedCallback(NetworkPlayerID lPlayerID, void* lpUserData);

        SignalMessage   maAckMessage[KI_MAX_ACKS_TO_BUFFER];                  // +0x1FF0
        SignalMessage   maNackMessage[KI_MAX_NACKS_TO_BUFFER];                // +0x2180
        PlayerData      maInactivePlayers[KI_MAX_PLAYERS];                    // +0x2310
        PlayerData      maActivePlayers[KI_MAX_PLAYERS];                      // +0x2370
        EventCallback   maEventCallbacks[KI_NUM_EVENT_CALLBACKS];             // +0x23D0
        s32             miNumEventCallbacks;                                  // +0x23D8
        EPrepareState   mePrepareState;                                       // +0x23DC
        s32             miNumActivePlayers;                                   // +0x23E0
        s32             miNumInactivePlayers;                                 // +0x23E4
        ServerInterface* mpServerInterface;                                   // +0x23E8
        NetworkAdapter* mpNetworkAdapter;                                     // +0x23EC
        TimeManager*    mpTimeManager;                                        // +0x23F0
        CgsSystem::EFrameRate meLocalConsoleFrameRate;                        // +0x23F4
        NetworkPlayerID mHostPlayerID;                                        // +0x23F8
        NetworkPlayerID mLocalPlayerID;                                       // +0x23FC
        PlayerManagerPrepareParams::OnReceivedFromWrongIPCallback*
                        mpfOnReceivedFromWrongIPCallback;                     // +0x2400
        PlayersConnectionManager::ConnMgrConnectionFinalisedCallback
                        mpfConnectionFinalisedCallback;                       // +0x2404
        void*           mpConnectionFinalisedUserData;                        // +0x2408
        u16             mu16CurrentFrame;                                     // +0x240C
        u8              mu8GameID;                                            // +0x240E
        bool            mbDiskAccessible;                                     // +0x240F
        bool            mbPlayerListIsValid;                                  // +0x2410
        s32             miPLAYERManagerSendMessagesPM;                        // +0x2414
        PlayerManagerDebugComponent mDebugComponent;                          // +0x2418
        // The committed PlayerManagerDebugComponent ends 4 console bytes short of its 0x24
        // span (its trailing mCompressionUtils member is not modelled yet).
        u8              maDebugComponentReserve[4];                           // +0x2438
        s32             miBytesUsedForAcks;                                   // +0x243C
        s32             miBytesUsedForUnreliableMessages;                     // +0x2440
        s32             miBytesUsedForReliableMessages;                       // +0x2444
        s32             miBytesUsedForReliableResendMessages;                 // +0x2448
        s32             maMessageBytes[KI_NUM_MESSAGE_BYTE_COUNTERS];         // +0x244C
    };
}
