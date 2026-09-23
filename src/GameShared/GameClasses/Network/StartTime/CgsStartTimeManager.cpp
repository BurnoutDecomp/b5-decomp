#include "types.hpp"
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"
#include "GameShared/GameClasses/Network/Time/CgsTimeManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::StartTimeManager -- the pre-start handshake. The host waits until every
// connected client reports ready (or the wait times out), picks a start time a short gap
// ahead of the network clock and sends it to every client. A client syncs its clock,
// reports ready to the host and waits for the start time to arrive; everyone then waits
// for the network clock to reach it. Host migration restarts the handshake under the new
// host.
//
// The original streams progress and error lines through a dev-log stream; those lines are
// dropped. Asserts whose message was built through the assert buffer keep the literal
// prefix of that message.

namespace CgsNetwork
{
    const CgsSystem::Time K_MIN_TIME_SPENT_SYNCING_TIME(1.0f);
    const CgsSystem::Time K_MAX_TIME_SPENT_SYNCING_TIME(30.0f);
    const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_START_TIME(30.0f);
    const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_SILENT_CLIENT_READY(30.0f);
    const CgsSystem::Time K_MAX_TIME_TO_WAIT_FOR_COMMUNICATING_CLIENT_READY(45.0f);
    const CgsSystem::Time K_TIME_GAP_TO_LEAVE_BEFORE_START_TIME(5.0f);

    // The message types this manager registers with each remote player.
    static const s32 KI_E_MESSAGE_TYPE_READY      = 1;
    static const s32 KI_E_MESSAGE_TYPE_START_TIME = 3;

// The console ctor only seeds the embedded members: the start-time and ready message
// objects of every slot and the time members (CgsSystem::Time's own ctor zeroes them).
StartTimeManager::StartTimeManager()
{
}

// Latch the collaborators, construct the clock and install the default timeouts.
void StartTimeManager::Construct(HostMigrationManager* lpHostMigrationManager,
                                 TimeManager* lpTimeManager,
                                 PlayerManager* lpPlayerManager,
                                 StartMessageArrivedLateCallback* lpfStartMessageArrivedLateCallback,
                                 ClientReadyCallback* lpfClientReadyCallback,
                                 void* lpClientReadyData)
{
    mpHostMigrationManager             = lpHostMigrationManager;
    mpTimeManager                      = lpTimeManager;
    mpPlayerManager                    = lpPlayerManager;
    mpfStartMessageArrivedLateCallback = lpfStartMessageArrivedLateCallback;
    mpfClientReadyCallback             = lpfClientReadyCallback;
    mpClientReadyData                  = lpClientReadyData;

    mpTimeManager->Construct(lpPlayerManager);

    SetStatus(CgsSystem::Time(0.0f), E_CONSTRUCTED);

    SetSyncTimeTimeouts(K_MIN_TIME_SPENT_SYNCING_TIME, K_MAX_TIME_SPENT_SYNCING_TIME);
    SetWaitForStartTimeTimeout(K_MAX_TIME_TO_WAIT_FOR_START_TIME);
    SetWaitForClientReadyTimeouts(K_MAX_TIME_TO_WAIT_FOR_SILENT_CLIENT_READY,
                                  K_MAX_TIME_TO_WAIT_FOR_COMMUNICATING_CLIENT_READY);
    SetGapTillStartTime(K_TIME_GAP_TO_LEAVE_BEFORE_START_TIME);
}

// Hook host migration, reset every slot and the start time, and prepare the clock.
bool StartTimeManager::Prepare()
{
    mpHostMigrationManager->RegisterHostMigrationCallback(OnHostMigrationCallback, this);

    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        MessageData& lMessageData = maMsgData[liPlayerIndex];
        lMessageData.mStartTimeMessageSend.Construct();
        lMessageData.mStartTimeMessageRecv.Construct();
        lMessageData.mReadyMessageSend.Construct();
        lMessageData.mReadyMessageRecv.Construct();
        lMessageData.mPlayerID        = -1;
        lMessageData.mbPlayerReady    = false;
        lMessageData.mbSentStartTime  = false;
    }

    PrepareStartTime();

    CGS_ASSERT(mpTimeManager->Prepare(), "mpTimeManager->Prepare()");
    return true;
}

// Forget any start time and go back to the constructed state.
void StartTimeManager::PrepareStartTime()
{
    mStartTime.mHostID = -1;
    mStartTime.mStartTime.SetFloatVal(0.0f);
    mbStartTimeIsValid      = false;
    mbKeepSyncingAfterStart = false;

    SetStatus(CgsSystem::Time(0.0f), E_CONSTRUCTED);
}

// Begin the handshake: nobody is ready and nothing has been sent yet.
void StartTimeManager::StartSyncingTime(bool lbKeepSyncingAfterStart)
{
    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        maMsgData[liPlayerIndex].mbPlayerReady   = false;
        maMsgData[liPlayerIndex].mbSentStartTime = false;
    }

    mbKeepSyncingAfterStart = lbKeepSyncingAfterStart;

    SetStatus(CgsSystem::Time(0.0f), E_START_SYNCING);
}

void StartTimeManager::StopSyncingTime()
{
    mpTimeManager->StopSyncingTime();
}

// Advance the clock, then the handshake state machine. Every transition stamps the
// state-entry time from the timer.
void StartTimeManager::Update(const CgsSystem::TimerStatus* lpTimerStatus)
{
    mpTimeManager->Update();

    switch (meStatus)
    {
    case E_CONSTRUCTED:
        break;

    case E_START_SYNCING:
        if (mpPlayerManager->AmIHost())
        {
            PrepareHostWaitForClientsReady();
            SetStatus(lpTimerStatus->GetTime(), E_HOST_WAIT_FOR_CLIENTS_READY);
        }
        else
        {
            PrepareClientSyncTime();
            SetStatus(lpTimerStatus->GetTime(), E_CLIENT_SYNC_TIME);
        }
        break;

    case E_HOST_WAIT_FOR_CLIENTS_READY:
        if (UpdateHostWaitForClientsReady(lpTimerStatus))
        {
            SetStatus(lpTimerStatus->GetTime(), E_HOST_SEND_START_TIME);
        }
        break;

    case E_HOST_SEND_START_TIME:
    {
        StartTime lStartTime;
        lStartTime.mStartTime = mpTimeManager->GetNetworkTime() + mGapToLeaveBeforeStartTime;
        mpPlayerManager->GetNextLocalPlayerID(&lStartTime.mHostID);

        SendStartTime(&lStartTime);
        SetStatus(lpTimerStatus->GetTime(), E_WAIT_FOR_START_TIME);
        break;
    }

    case E_CLIENT_SYNC_TIME:
        if (UpdateClientSyncTime(lpTimerStatus))
        {
            SetStatus(lpTimerStatus->GetTime(), E_CLIENT_SEND_READY);
        }
        break;

    case E_CLIENT_SEND_READY:
    {
        const NetworkPlayerID lHostID = mpPlayerManager->GetHostPlayerID();

        s32 liPlayerIndex;
        for (liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
        {
            MessageData& lMessageData = maMsgData[liPlayerIndex];
            if (lMessageData.mPlayerID != -1 && lMessageData.mPlayerID == lHostID)
            {
                lMessageData.mReadyMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount());
                break;
            }
        }

        // The original also dev-logs every slot's player id when the host is missing.
        CGS_ASSERT(liPlayerIndex < KI_MAX_MESSAGE_DATA, "lHostID=");

        SetStatus(lpTimerStatus->GetTime(), E_CLIENT_WAIT_START_TIME_ARRIVE);
        break;
    }

    case E_CLIENT_WAIT_START_TIME_ARRIVE:
        if (UpdateClientWaitStartTimeArrive(lpTimerStatus))
        {
            SetStatus(lpTimerStatus->GetTime(), E_WAIT_FOR_START_TIME);
        }
        break;

    case E_WAIT_FOR_START_TIME:
        if (UpdateWaitForStartTime(lpTimerStatus))
        {
            SetStatus(lpTimerStatus->GetTime(), E_STARTED);
        }
        break;

    case E_STARTED:
        UpdateStarted();
        break;

    default:
        // The original appends the state number.
        CGS_ASSERT(false, "Start time manager in unknown state ");
        break;
    }
}

// Unhook host migration, unregister every player, reset the slots and release the clock.
bool StartTimeManager::Release()
{
    mpHostMigrationManager->UnregisterHostMigrationCallback(OnHostMigrationCallback);
    UnregisterMessages();

    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        MessageData& lMessageData = maMsgData[liPlayerIndex];
        lMessageData.mStartTimeMessageSend.Construct();
        lMessageData.mStartTimeMessageRecv.Construct();
        lMessageData.mReadyMessageSend.Construct();
        lMessageData.mReadyMessageRecv.Construct();
        lMessageData.mPlayerID        = -1;
        lMessageData.mbPlayerReady    = false;
        lMessageData.mbSentStartTime  = false;
    }

    mStartTime.mHostID = -1;
    mStartTime.mStartTime.SetFloatVal(0.0f);
    mbStartTimeIsValid = false;

    mpTimeManager->Release();

    SetStatus(CgsSystem::Time(0.0f), E_CONSTRUCTED);
    return true;
}

// Host migration: a former host becomes a client; a client re-evaluates its role. The
// other states are left alone.
void StartTimeManager::OnHostMigration(const CgsSystem::TimerStatus* lpTimerStatus,
                                       NetworkPlayerID lOldHostID, NetworkPlayerID lNewHostID)
{
    NetworkPlayerID lLocalPlayerID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
    CGS_ASSERT(lLocalPlayerID != K_INVALID_PLAYER_ID, "lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    mpTimeManager->OnHostMigration(lOldHostID, lNewHostID, lLocalPlayerID == lNewHostID);

    switch (meStatus)
    {
    case E_HOST_WAIT_FOR_CLIENTS_READY:
    case E_HOST_SEND_START_TIME:
        CGS_ASSERT(!mpPlayerManager->AmIHost(), "!mpPlayerManager->AmIHost()");
        PrepareClientSyncTime();
        SetStatus(lpTimerStatus->GetTime(), E_CLIENT_SYNC_TIME);
        break;

    case E_CLIENT_SYNC_TIME:
    case E_CLIENT_WAIT_START_TIME_ARRIVE:
        if (mpPlayerManager->AmIHost())
        {
            PrepareHostWaitForClientsReady();
            SetStatus(lpTimerStatus->GetTime(), E_HOST_WAIT_FOR_CLIENTS_READY);
        }
        else
        {
            PrepareClientSyncTime();
            SetStatus(lpTimerStatus->GetTime(), E_CLIENT_SYNC_TIME);
        }
        break;

    default:
        // The original dev-logs that no action is taken in this state.
        break;
    }
}

// A player joined: tell the clock, and register the handshake messages with every
// player other than ourselves.
void StartTimeManager::AddPlayer(NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(mpTimeManager, "mpTimeManager");

    NetworkPlayerID lLocalPlayerID;
    bool lbIAmHost = false;
    if (mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID))
    {
        lbIAmHost = mpPlayerManager->AmIHost();
    }

    mpTimeManager->AddPlayer(lPlayerID, lbIAmHost);

    if (lPlayerID != lLocalPlayerID)
    {
        RegisterMessages(lPlayerID);
    }
}

// A player left: tell the clock, and drop the player's handshake messages.
void StartTimeManager::RemovePlayer(NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(mpTimeManager, "mpTimeManager");

    mpTimeManager->RemovePlayer(lPlayerID);

    NetworkPlayerID lLocalPlayerID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);
    if (lPlayerID != lLocalPlayerID)
    {
        UnregisterMessages(lPlayerID);
    }
}

// Lost the session: reset the clock and the sync message pool, drop every player's
// messages and go back to the constructed state.
void StartTimeManager::Disconnected()
{
    CGS_ASSERT(mpTimeManager, "mpTimeManager");
    mpTimeManager->Disconnected();

    UnregisterMessages();

    SetStatus(CgsSystem::Time(0.0f), E_CONSTRUCTED);
}

// Syncing matters to clients until the start time has passed; the host is the reference.
bool StartTimeManager::AreWeSyncingTime()
{
    if (mpPlayerManager->AmIHost())
    {
        return false;
    }

    return meStatus != E_STARTED;
}

// Start now: the current host and the timer's current time become the start time.
void StartTimeManager::ForceStartTime(const CgsSystem::TimerStatus* lpTimerStatus)
{
    mStartTime.mHostID    = mpPlayerManager->GetHostPlayerID();
    mStartTime.mStartTime = lpTimerStatus->GetTime();
    meStatus              = E_STARTED;
    mbStartTimeIsValid    = true;
}

// Claim a free slot for the player and register its start-time and ready messages.
void StartTimeManager::RegisterMessages(NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

    NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
    CGS_ASSERT(lpNetPlayer, "lpNetPlayer");

    for (s32 liIndex = 0; liIndex < KI_MAX_MESSAGE_DATA; ++liIndex)
    {
        CGS_ASSERT(maMsgData[liIndex].mPlayerID != lPlayerID, "maMsgData[liIndex].mPlayerID != lPlayerID");
    }

    s32 liFreeSlot;
    for (liFreeSlot = 0; liFreeSlot < KI_MAX_MESSAGE_DATA; ++liFreeSlot)
    {
        if (maMsgData[liFreeSlot].mPlayerID == -1)
        {
            break;
        }
    }
    CGS_ASSERT(liFreeSlot < KI_MAX_MESSAGE_DATA, "liFreeSlot<KI_MAX_NETWORK_PLAYERS");

    MessageData& lMessageData = maMsgData[liFreeSlot];
    lMessageData.mPlayerID       = lPlayerID;
    lMessageData.mbPlayerReady   = false;
    lMessageData.mbSentStartTime = false;

    lpNetPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_START_TIME, sizeof(StartTimeMessage),
                                     &lMessageData.mStartTimeMessageSend,
                                     &lMessageData.mStartTimeMessageRecv,
                                     StartTimeMessageArrivedCallback,
                                     StartTimeMessageDeliveredCallback,
                                     this);
    lpNetPlayer->RegisterMessageType(KI_E_MESSAGE_TYPE_READY, sizeof(ReadyMessage),
                                     &lMessageData.mReadyMessageSend,
                                     &lMessageData.mReadyMessageRecv,
                                     ReadyMessageArrivedCallback,
                                     ReadyMessageDeliveredCallback,
                                     this);
}

// Unregister every player's handshake messages.
void StartTimeManager::UnregisterMessages()
{
    CGS_ASSERT(mpPlayerManager, "mpPlayerManager");

    NetworkPlayerID lPlayerID = -1;
    while (mpPlayerManager->GetNextPlayerID(&lPlayerID, PlayerManager::E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
        if (lpNetPlayer)
        {
            UnregisterMessages(lPlayerID);
        }
    }
}

// Unregister the player's start-time and ready messages and free its slot.
void StartTimeManager::UnregisterMessages(NetworkPlayerID lPlayerID)
{
    NetworkPlayer* lpNetPlayer = mpPlayerManager->GetPlayerByID(lPlayerID);
    CGS_ASSERT(lpNetPlayer, "lpNetPlayer");

    for (s32 liIndex = 0; liIndex < KI_MAX_MESSAGE_DATA; ++liIndex)
    {
        MessageData& lMessageData = maMsgData[liIndex];
        if (lMessageData.mPlayerID != -1 && lMessageData.mPlayerID == lPlayerID)
        {
            if (lpNetPlayer->IsMessageTypeRegistered(KI_E_MESSAGE_TYPE_START_TIME))
            {
                lpNetPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_START_TIME);
            }
            if (lpNetPlayer->IsMessageTypeRegistered(KI_E_MESSAGE_TYPE_READY))
            {
                lpNetPlayer->UnRegisterMessageType(KI_E_MESSAGE_TYPE_READY);
            }

            lMessageData.mPlayerID = -1;
            return;
        }
    }
}

// Take a start time. A second one is only accepted from a host that outranks the first.
void StartTimeManager::SetStartTime(const StartTime* lpStartTime)
{
    CGS_ASSERT(!IsStartTimeValid()
               || mpHostMigrationManager->ABecomesHostBeforeB(mStartTime.mHostID, lpStartTime->mHostID),
               "!IsStartTimeValid() || mpHostMigrationManager->ABecomesHostBeforeB(mStartTime.mHostID, lpStartTime->mHostID)");

    mbStartTimeIsValid = true;
    mStartTime         = *lpStartTime;
}

StartTimeManager::EPlayerReadiness StartTimeManager::AreAllPlayersReadyToStart(
    const CgsSystem::TimerStatus* lpTimerStatus,
    CgsSystem::Time,
    CgsSystem::Time lTimeout)
{
    if (lpTimerStatus->GetTime() >= lTimeout)
        return E_TIMEOUT;

    NetworkPlayerID liPlayerID = -1;

    while (mpPlayerManager->GetNextPlayerID(&liPlayerID, PlayerManager::E_CONSIDER_ALL_PLAYERS))
    {
        NetworkPlayer* lpPlayer = mpPlayerManager->GetPlayerByID(liPlayerID);
        if (!lpPlayer)
            continue;

        if (mpPlayerManager->mConnectionManager.GetConnectionStatus(liPlayerID) != E_CONNECTION_SUCCESS)
            return E_NOT_READY;

        if (lpPlayer->mbNetworkPlayerPaused || lpPlayer->HasConnectionFailed())
            continue;

        bool lbPlayerReady = false;
        for (s32 liIndex = 0; liIndex < KI_MAX_MESSAGE_DATA; ++liIndex)
        {
            const MessageData& lMessageData = maMsgData[liIndex];
            if (lMessageData.mPlayerID != -1
                && lMessageData.mPlayerID == liPlayerID
                && lMessageData.mbPlayerReady)
            {
                lbPlayerReady = true;
                break;
            }
        }

        if (!lbPlayerReady)
            return E_NOT_READY;
    }

    return E_READY;
}

// Take the start time and send it to every connected player.
void StartTimeManager::SendStartTime(const StartTime* lpStartTime)
{
    SetStartTime(lpStartTime);

    NetworkPlayerID laReceivedClientsIDs[KI_START_TIME_CLIENT_COUNT];
    for (s32 liClientIndex = 0; liClientIndex < KI_START_TIME_CLIENT_COUNT; ++liClientIndex)
    {
        laReceivedClientsIDs[liClientIndex] = -1;
    }

    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        MessageData& lMessageData = maMsgData[liPlayerIndex];
        if (lMessageData.mPlayerID != -1
            && mpPlayerManager->mConnectionManager.GetConnectionStatus(lMessageData.mPlayerID) == E_CONNECTION_SUCCESS)
        {
            lMessageData.mStartTimeMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                              &mStartTime, laReceivedClientsIDs);
            lMessageData.mbSentStartTime = true;
        }
    }
}

// The host and the client start by syncing the clock (the two are identical code).
void StartTimeManager::PrepareHostWaitForClientsReady()
{
    mpTimeManager->StartSyncingTime();
}

// Done when every player is ready or the wait timed out, or when a start time from a host
// that outranks us is already in hand. Clock syncing stops unless it is to go on after the
// start.
bool StartTimeManager::UpdateHostWaitForClientsReady(const CgsSystem::TimerStatus* lpTimerStatus)
{
    NetworkPlayerID lLocalPlayerID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    if (IsStartTimeValid() && mpHostMigrationManager->ABecomesHostBeforeB(lLocalPlayerID, mStartTime.mHostID))
    {
        mpTimeManager->StopSyncingTime();
        return true;
    }

    const CgsSystem::Time lCommunicatingPlayersTimeout = mStatusEnterTime + mTimeToWaitForCommunicatingClientReady;
    const CgsSystem::Time lSilentPlayersTimeout        = mStatusEnterTime + mTimeToWaitForSilentClientReady;

    const EPlayerReadiness leReadiness =
        AreAllPlayersReadyToStart(lpTimerStatus, lSilentPlayersTimeout, lCommunicatingPlayersTimeout);
    if (leReadiness != E_READY && leReadiness != E_TIMEOUT)
    {
        return false;
    }

    if (!mbKeepSyncingAfterStart)
    {
        mpTimeManager->StopSyncingTime();
    }
    return true;
}

void StartTimeManager::PrepareClientSyncTime()
{
    mpTimeManager->StartSyncingTime();
}

// Done once the clock is in sync (or a start time already arrived) and the minimum sync
// time has passed, or once the maximum sync time has passed. Clock syncing then stops.
bool StartTimeManager::UpdateClientSyncTime(const CgsSystem::TimerStatus* lpTimerStatus)
{
    bool lbDone = false;

    if (mpTimeManager->IsTimeSynchronised()
        && (lpTimerStatus->GetTime() - mStatusEnterTime) > mMinTimeToSyncTime)
    {
        lbDone = true;
    }
    else if (IsStartTimeValid()
             && (lpTimerStatus->GetTime() - mStatusEnterTime) > mMinTimeToSyncTime)
    {
        CGS_ASSERT(!mpPlayerManager->AmIHost(), "!mpPlayerManager->AmIHost()");
        lbDone = true;
    }
    else if ((lpTimerStatus->GetTime() - mStatusEnterTime) > mMaxTimeToSyncTime)
    {
        lbDone = true;
    }

    if (!lbDone)
    {
        return false;
    }

    mpTimeManager->StopSyncingTime();
    return true;
}

// Done once the start time arrived. If it has not arrived within the wait, the client
// makes its own: the network clock plus the default gap, under its own id.
bool StartTimeManager::UpdateClientWaitStartTimeArrive(const CgsSystem::TimerStatus* lpTimerStatus)
{
    if (IsStartTimeValid())
    {
        return true;
    }

    if ((lpTimerStatus->GetTime() - mStatusEnterTime) > mTimeToWaitForStartTime)
    {
        StartTime lStartTime;
        lStartTime.mStartTime = mpTimeManager->GetNetworkTime() + K_TIME_GAP_TO_LEAVE_BEFORE_START_TIME;
        mpPlayerManager->GetNextLocalPlayerID(&lStartTime.mHostID);

        SetStartTime(&lStartTime);
        return true;
    }

    return false;
}

// Done once the network clock reaches the start time.
bool StartTimeManager::UpdateWaitForStartTime(const CgsSystem::TimerStatus*)
{
    return mpTimeManager->GetNetworkTime() >= mStartTime.mStartTime;
}

// After the start the host keeps sending the start time to players that became ready but
// have not had it yet (only while syncing is to go on after the start).
void StartTimeManager::UpdateStarted()
{
    if (!mbKeepSyncingAfterStart || !mpPlayerManager->AmIHost())
    {
        return;
    }

    for (s32 liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        MessageData& lMessageData = maMsgData[liPlayerIndex];
        if (lMessageData.mPlayerID != -1
            && lMessageData.mbPlayerReady
            && !lMessageData.mbSentStartTime
            && mpPlayerManager->mConnectionManager.GetConnectionStatus(lMessageData.mPlayerID) == E_CONNECTION_SUCCESS)
        {
            NetworkPlayerID laReceivedClientsIDs[KI_START_TIME_CLIENT_COUNT];
            for (s32 liClientIndex = 0; liClientIndex < KI_START_TIME_CLIENT_COUNT; ++liClientIndex)
            {
                laReceivedClientsIDs[liClientIndex] = -1;
            }

            lMessageData.mStartTimeMessageSend.PrepareForSend(mpTimeManager->GetU16FrameCount(),
                                                              &mStartTime, laReceivedClientsIDs);
            lMessageData.mbSentStartTime = true;
        }
    }
}

// A start time arrived. It is ignored once started; otherwise it is taken unless we
// already hold one from a host that outranks the sender's.
void StartTimeManager::StartTimeMessageArrivedCallback(ReliableMessage* lpMessage,
                                                       NetworkPlayerID lSendingPlayerID,
                                                       void* lpUserData)
{
    StartTimeMessage*     lpStartTimeMessage     = static_cast<StartTimeMessage*>(lpMessage);
    StartTime             lStartTime;
    StartTimeManager*     lpStartTimeManager     = static_cast<StartTimeManager*>(lpUserData);
    HostMigrationManager* lpHostMigrationManager = lpStartTimeManager->mpHostMigrationManager;
    TimeManager*          lpTimeManager          = lpStartTimeManager->mpTimeManager;
    NetworkPlayerID       laReceivedClientsIDs[KI_START_TIME_CLIENT_COUNT];

    if (lpStartTimeManager->meStatus == E_STARTED)
    {
        // The original dev-logs that the message is ignored and the start time in use.
        return;
    }

    const bool lbSuccess = lpStartTimeMessage->Retrieve(&lStartTime, laReceivedClientsIDs);
    CGS_ASSERT(lbSuccess, "lbSuccess");

    if (lpStartTimeManager->IsStartTimeValid()
        && !lpHostMigrationManager->ABecomesHostBeforeB(lpStartTimeManager->mStartTime.mHostID, lStartTime.mHostID))
    {
        return;
    }

    if (lpTimeManager->GetNetworkTime() > lStartTime.mStartTime)
    {
        if (lpStartTimeManager->mpfStartMessageArrivedLateCallback)
        {
            lpStartTimeManager->mpfStartMessageArrivedLateCallback();
        }
    }

    // Add the sender to the acknowledged list (the list is not used further).
    s32 liPlayerIndex;
    for (liPlayerIndex = 0; liPlayerIndex < KI_START_TIME_CLIENT_COUNT; ++liPlayerIndex)
    {
        if (laReceivedClientsIDs[liPlayerIndex] == -1)
        {
            laReceivedClientsIDs[liPlayerIndex] = lSendingPlayerID;
            break;
        }
    }
    CGS_ASSERT(liPlayerIndex < KI_START_TIME_CLIENT_COUNT, "liPlayerIndex<KI_MAX_PLAYERS");

    lpStartTimeManager->SetStartTime(&lStartTime);
}

// Delivery notifications are not used.
void StartTimeManager::StartTimeMessageDeliveredCallback(bool, bool, SignalMessage*, NetworkPlayerID, void*)
{
}

// A client reported ready: mark its slot and tell the game.
void StartTimeManager::ReadyMessageArrivedCallback(ReliableMessage*,
                                                   NetworkPlayerID lSendingPlayerID,
                                                   void* lpUserData)
{
    StartTimeManager* lpStartTimeMgr = static_cast<StartTimeManager*>(lpUserData);
    CGS_ASSERT(lpStartTimeMgr, "lpStartTimeMgr");

    s32 liPlayerIndex;
    for (liPlayerIndex = 0; liPlayerIndex < KI_MAX_MESSAGE_DATA; ++liPlayerIndex)
    {
        MessageData& lMessageData = lpStartTimeMgr->maMsgData[liPlayerIndex];
        if (lMessageData.mPlayerID != -1 && lMessageData.mPlayerID == lSendingPlayerID)
        {
            lMessageData.mbPlayerReady = true;
            if (lpStartTimeMgr->mpfClientReadyCallback)
            {
                lpStartTimeMgr->mpfClientReadyCallback(lSendingPlayerID, lpStartTimeMgr->mpClientReadyData);
            }
            break;
        }
    }

    CGS_ASSERT(liPlayerIndex < KI_MAX_MESSAGE_DATA, "liPlayerIndex < KI_MAX_NETWORK_PLAYERS");
}

// Delivery notifications are not used.
void StartTimeManager::ReadyMessageDeliveredCallback(bool, bool, SignalMessage*, NetworkPlayerID, void*)
{
}

void StartTimeManager::OnHostMigrationCallback(const CgsSystem::TimerStatus* lpTimerStatus,
                                               NetworkPlayerID lOldHostID,
                                               NetworkPlayerID lNewHostID,
                                               void* lpUserData)
{
    static_cast<StartTimeManager*>(lpUserData)->OnHostMigration(lpTimerStatus, lOldHostID, lNewHostID);
}
}
