#pragma once

// ===================================================================================
// CgsNetwork::StartTimeManager -- owning header
//   b5-decomp/src/GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h
//
// The pre-start synchronisation state machine (embedded in CgsNetwork::NetworkManager at
// +0x2B68). The host waits for every client to report ready, then broadcasts the agreed
// start time; clients sync their clock, report ready and wait for the start time to
// arrive. Host migration re-runs the handshake under the new host.
//
// Console layout (0x768 bytes; Construct stores the six pointer words, the ctor and
// Construct seed the time members, the per-slot walks stride 0x100):
//   +0x000  maMsgData[7]                        (MessageData, 0x100 each)
//   +0x700  mpHostMigrationManager
//   +0x704  mpTimeManager
//   +0x708  mStartTime                          (StartTime, 12)
//   +0x714  mpPlayerManager
//   +0x718  mpfStartMessageArrivedLateCallback
//   +0x71C  mpfClientReadyCallback
//   +0x720  mpClientReadyData
//   +0x724  mbStartTimeIsValid
//   +0x728  meStatus
//   +0x72C  mStatusEnterTime
//   +0x734  mMinTimeToSyncTime
//   +0x73C  mMaxTimeToSyncTime
//   +0x744  mTimeToWaitForStartTime
//   +0x74C  mTimeToWaitForSilentClientReady
//   +0x754  mTimeToWaitForCommunicatingClientReady
//   +0x75C  mGapToLeaveBeforeStartTime
//   +0x764  mbKeepSyncingAfterStart
//
// MessageData (0x100 bytes): StartTimeMessage send/recv (0x54 each), ReadyMessage
// send/recv (0x28 each), then the slot's player id at +0xF8, mbPlayerReady at +0xFC and
// mbSentStartTime at +0xFD.
//
// Members are reached by name; the offsets above are console facts (the host widens
// every pointer).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameShared/GameClasses/Network/Time/CgsStartTimeMessage.h"      // StartTime, StartTimeMessage
#include "GameShared/GameClasses/Network/Time/CgsReadyMessage.h"

namespace CgsSystem
{
    class TimerStatus;
}

namespace CgsNetwork
{
    struct HostMigrationManager;
    struct TimeManager;
    struct PlayerManager;
    struct ReliableMessage;
    struct SignalMessage;

    typedef s32 NetworkPlayerID;

    // The default handshake timeouts Construct installs (seconds: 1, 30, 30, 30, 45, 5).
    extern const CgsSystem::Time K_MIN_TIME_SPENT_SYNCING_TIME;
    extern const CgsSystem::Time K_MAX_TIME_SPENT_SYNCING_TIME;
    extern const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_START_TIME;
    extern const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_SILENT_CLIENT_READY;
    extern const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_COMMUNICATING_CLIENT_READY;
    extern const CgsSystem::Time K_TIME_GAP_TO_LEAVE_BEFORE_START_TIME;

    struct StartTimeManager
    {
        static const s32 KI_MAX_MESSAGE_DATA = 7;

        struct MessageData
        {
            StartTimeMessage mStartTimeMessageSend;   // +0x00
            StartTimeMessage mStartTimeMessageRecv;   // +0x54
            ReadyMessage     mReadyMessageSend;       // +0xA8
            ReadyMessage     mReadyMessageRecv;       // +0xD0
            NetworkPlayerID  mPlayerID;               // +0xF8
            bool             mbPlayerReady;           // +0xFC
            bool             mbSentStartTime;         // +0xFD
        };

        enum EPlayerReadiness
        {
            E_NOT_READY       = 0,
            E_READY           = 1,
            E_TIMEOUT         = 2,
            E_READINESS_COUNT = 3,
        };

        enum EPreStartSyncStatus
        {
            E_CONSTRUCTED                   = 0,
            E_START_SYNCING                 = 1,
            E_HOST_WAIT_FOR_CLIENTS_READY   = 2,
            E_HOST_SEND_START_TIME          = 3,
            E_CLIENT_SYNC_TIME              = 4,
            E_CLIENT_SEND_READY             = 5,
            E_CLIENT_WAIT_START_TIME_ARRIVE = 6,
            E_WAIT_FOR_START_TIME           = 7,
            E_STARTED                       = 8,
            E_SYNCING_STATUS_COUNT          = 9,
        };

        typedef void StartMessageArrivedLateCallback();
        typedef void ClientReadyCallback(NetworkPlayerID lClientReadyID, void* lpUserData);

        StartTimeManager();

        void Construct(HostMigrationManager* lpHostMigrationManager, TimeManager* lpTimeManager,
                       PlayerManager* lpPlayerManager,
                       StartMessageArrivedLateCallback* lpfStartMessageArrivedLateCallback,
                       ClientReadyCallback* lpfClientReadyCallback, void* lpClientReadyData);
        bool Prepare();
        void PrepareStartTime();
        void StartSyncingTime(bool lbKeepSyncingAfterStart);
        void StopSyncingTime();
        void Update(const CgsSystem::TimerStatus* lpTimerStatus);
        bool Release();
        void Destruct();

        bool HasStartTimePassed() const { return meStatus == E_STARTED; }
        const StartTime* GetStartTime() const { return mbStartTimeIsValid ? &mStartTime : nullptr; }

        void OnHostMigration(const CgsSystem::TimerStatus* lpTimerStatus,
                             NetworkPlayerID lOldHostID, NetworkPlayerID lNewHostID);
        void AddPlayer(NetworkPlayerID lPlayerID);
        void RemovePlayer(NetworkPlayerID lPlayerID);
        void Disconnected();

        void SetSyncTimeTimeouts(CgsSystem::Time lMinTime, CgsSystem::Time lMaxTime)
        {
            mMinTimeToSyncTime = lMinTime;
            mMaxTimeToSyncTime = lMaxTime;
        }
        void SetWaitForStartTimeTimeout(CgsSystem::Time lTimeout)
        {
            mTimeToWaitForStartTime = lTimeout;
        }
        void SetWaitForClientReadyTimeouts(CgsSystem::Time lSilentTimeout,
                                           CgsSystem::Time lCommunicatingTimeout)
        {
            mTimeToWaitForSilentClientReady        = lSilentTimeout;
            mTimeToWaitForCommunicatingClientReady = lCommunicatingTimeout;
        }
        void SetGapTillStartTime(CgsSystem::Time lGap)
        {
            mGapToLeaveBeforeStartTime = lGap;
        }

        bool AreWeSyncingTime();
        void ForceStartTime(const CgsSystem::TimerStatus* lpTimerStatus);

    private:
        void RegisterMessages(NetworkPlayerID lPlayerID);
        void UnregisterMessages();
        void UnregisterMessages(NetworkPlayerID lPlayerID);
        bool IsStartTimeValid() const;
        void SetStartTime(const StartTime* lpStartTime);
        EPlayerReadiness AreAllPlayersReadyToStart(const CgsSystem::TimerStatus* lpTimerStatus,
                                                   CgsSystem::Time lMinTime,
                                                   CgsSystem::Time lTimeout);
        void SendStartTime(const StartTime* lpStartTime);
        void PrepareHostWaitForClientsReady();
        bool UpdateHostWaitForClientsReady(const CgsSystem::TimerStatus* lpTimerStatus);
        void PrepareClientSyncTime();
        bool UpdateClientSyncTime(const CgsSystem::TimerStatus* lpTimerStatus);
        void PrepareClientWaitStartTimeArrive();
        bool UpdateClientWaitStartTimeArrive(const CgsSystem::TimerStatus* lpTimerStatus);
        void PrepareWaitForStartTime();
        bool UpdateWaitForStartTime(const CgsSystem::TimerStatus* lpTimerStatus);
        void SetStatus(CgsSystem::Time lTime, EPreStartSyncStatus leStatus);
        void UpdateStarted();

        // Message and host-migration callbacks, registered by address (user data = the
        // manager).
        static void StartTimeMessageArrivedCallback(ReliableMessage* lpMessage,
                                                    NetworkPlayerID lSendingPlayerID,
                                                    void* lpUserData);
        static void StartTimeMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                      SignalMessage* lpMessage,
                                                      NetworkPlayerID lPlayerID, void* lpUserData);
        static void ReadyMessageArrivedCallback(ReliableMessage* lpMessage,
                                                NetworkPlayerID lSendingPlayerID,
                                                void* lpUserData);
        static void ReadyMessageDeliveredCallback(bool lbDelivered, bool lbWasReliable,
                                                  SignalMessage* lpMessage,
                                                  NetworkPlayerID lPlayerID, void* lpUserData);
        static void OnHostMigrationCallback(const CgsSystem::TimerStatus* lpTimerStatus,
                                            NetworkPlayerID lOldHostID,
                                            NetworkPlayerID lNewHostID, void* lpUserData);

        MessageData                      maMsgData[KI_MAX_MESSAGE_DATA];            // +0x000
        HostMigrationManager*            mpHostMigrationManager;                    // +0x700
        TimeManager*                     mpTimeManager;                             // +0x704
        StartTime                        mStartTime;                                // +0x708
        PlayerManager*                   mpPlayerManager;                           // +0x714
        StartMessageArrivedLateCallback* mpfStartMessageArrivedLateCallback;        // +0x718
        ClientReadyCallback*             mpfClientReadyCallback;                    // +0x71C
        void*                            mpClientReadyData;                         // +0x720
        bool                             mbStartTimeIsValid;                        // +0x724
        EPreStartSyncStatus              meStatus;                                  // +0x728
        CgsSystem::Time                  mStatusEnterTime;                          // +0x72C
        CgsSystem::Time                  mMinTimeToSyncTime;                        // +0x734
        CgsSystem::Time                  mMaxTimeToSyncTime;                        // +0x73C
        CgsSystem::Time                  mTimeToWaitForStartTime;                   // +0x744
        CgsSystem::Time                  mTimeToWaitForSilentClientReady;           // +0x74C
        CgsSystem::Time                  mTimeToWaitForCommunicatingClientReady;    // +0x754
        CgsSystem::Time                  mGapToLeaveBeforeStartTime;                // +0x75C
        bool                             mbKeepSyncingAfterStart;                   // +0x764
    };

    // Inline on the console (the network manager's Destruct carries the copy): drop the
    // six collaborator pointers.
    inline void StartTimeManager::Destruct()
    {
        mpHostMigrationManager             = nullptr;
        mpTimeManager                      = nullptr;
        mpPlayerManager                    = nullptr;
        mpfStartMessageArrivedLateCallback = nullptr;
        mpfClientReadyCallback             = nullptr;
        mpClientReadyData                  = nullptr;
    }

    inline bool StartTimeManager::IsStartTimeValid() const
    {
        return mbStartTimeIsValid;
    }

    inline void StartTimeManager::SetStatus(CgsSystem::Time lTime, EPreStartSyncStatus leStatus)
    {
        mStatusEnterTime = lTime;
        meStatus         = leStatus;
    }
}
