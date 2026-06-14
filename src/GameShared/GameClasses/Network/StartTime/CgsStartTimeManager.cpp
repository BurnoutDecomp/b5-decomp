#include "types.hpp"

namespace CgsNetwork
{
namespace
{
const u32 KU_START_TIME_MESSAGE_VTABLE = 0x820CF48C;
const u32 KU_READY_MESSAGE_VTABLE = 0x820CF4B4;
const s32 KI_MAX_START_TIME_PLAYERS = 7;
const s32 KI_CONNECTED_STATUS = 3;

struct Time
{
    s32 miSeconds;
    f32 mfFraction;

    bool operator>=(const Time& lOther) const
    {
        return miSeconds > lOther.miSeconds
            || (miSeconds == lOther.miSeconds && mfFraction >= lOther.mfFraction);
    }
};

struct TimerStatus
{
    u8   mPad0[16];
    Time mTime;
};

struct NetworkPlayer
{
    u8   mPad0[0xBA8];
    bool mbNetworkPlayerPaused;
    bool mbHasConnectionFailed;
};

class PlayersConnectionManager
{
public:
    s32 GetConnectionStatus(s32 liPlayerID);

private:
    u8 mPad0[3864];
};

class PlayerManager : public PlayersConnectionManager
{
public:
    bool GetNextPlayerID(s32* pPlayerID, s32 lePlayersToConsider) const;

    struct PlayerData
    {
        u32 muPlayer;
        s32 miPlayerID;
        u32 muPad0;

        NetworkPlayer* GetPlayer() const
        {
            return reinterpret_cast<NetworkPlayer*>(static_cast<usize>(muPlayer));
        }
    };

private:
    u8         mPadAfterConnectionManager[5212];
    PlayerData maActivePlayers[8];
    u8         mPadAfterActivePlayers[12];
    s32        miNumActivePlayers;

public:
    const PlayerData* FindActivePlayer(s32 liPlayerID) const
    {
        for (s32 liIndex = 0; liIndex < miNumActivePlayers; ++liIndex)
        {
            if (maActivePlayers[liIndex].miPlayerID == liPlayerID)
                return &maActivePlayers[liIndex];
        }

        return 0;
    }
};

static_assert(sizeof(PlayersConnectionManager) == 3864, "PlayersConnectionManager layout drift");
static_assert(sizeof(PlayerManager::PlayerData) == 12, "PlayerData layout drift");
}

class StartTimeManager
{
public:
    enum EPlayerReadiness
    {
        E_NOT_READY = 0,
        E_READY = 1,
        E_TIMEOUT = 2
    };

    StartTimeManager();
    EPlayerReadiness AreAllPlayersReadyTo(const TimerStatus* lpTimerStatus, const void* lpUnused, const Time* lpTimeout);

    struct StartTimeMessage
    {
        u32 muVTable;
        u8  mPad4[40];
        Time mTime;
        u8  mPad52[32];
    };

    struct ReadyMessage
    {
        u32 muVTable;
        u8  mPad4[36];
    };

    struct MessageData
    {
        StartTimeMessage mStartTimeMessageSend;
        StartTimeMessage mStartTimeMessageRecv;
        ReadyMessage     mReadyMessageSend;
        ReadyMessage     mReadyMessageRecv;
        s32              miPlayerID;
        bool             mbPlayerReady;
        bool             mbSentStartTime;
        u8               mPadFE[2];
    };

    struct StartTime
    {
        u8   mPad0[4];
        Time mTime;
    };

private:
    MessageData maMsgData[KI_MAX_START_TIME_PLAYERS];
    u32         muHostMigrationManager;
    u32         muTimeManager;
    StartTime   mStartTime;
    u32         muPlayerManager;
    u32         muStartMessageArrivedLateCallback;
    u32         muClientReadyCallback;
    u32         muClientReadyData;
    bool        mbStartTimeIsValid;
    u8          mPad725[3];
    s32         meStatus;
    Time        mStatusEnterTime;
    Time        mMinTimeToSyncTime;
    Time        mMaxTimeToSyncTime;
    Time        mTimeToWaitForStartTime;
    Time        mTimeToWaitForSilentClientReady;
    Time        mTimeToWaitForCommunicatingClientReady;
    Time        mGapToLeaveBeforeStartTime;
    bool        mbKeepSyncingAfterStart;

    PlayerManager* GetPlayerManager() const
    {
        return reinterpret_cast<PlayerManager*>(static_cast<usize>(muPlayerManager));
    }

    bool IsPlayerReady(s32 liPlayerID) const
    {
        for (s32 liIndex = 0; liIndex < KI_MAX_START_TIME_PLAYERS; ++liIndex)
        {
            const MessageData& lMessageData = maMsgData[liIndex];
            if (lMessageData.miPlayerID != -1
                && lMessageData.miPlayerID == liPlayerID
                && lMessageData.mbPlayerReady)
            {
                return true;
            }
        }

        return false;
    }
};

static_assert(sizeof(StartTimeManager::StartTimeMessage) == 84, "StartTimeMessage layout drift");
static_assert(sizeof(StartTimeManager::ReadyMessage) == 40, "ReadyMessage layout drift");
static_assert(sizeof(StartTimeManager::MessageData) == 0x100, "MessageData layout drift");
static_assert(sizeof(StartTimeManager::StartTime) == 12, "StartTime layout drift");

StartTimeManager::StartTimeManager()
{
    for (s32 liIndex = 0; liIndex < KI_MAX_START_TIME_PLAYERS; ++liIndex)
    {
        MessageData& lMessageData = maMsgData[liIndex];

        lMessageData.mStartTimeMessageSend.muVTable = KU_START_TIME_MESSAGE_VTABLE;
        lMessageData.mStartTimeMessageSend.mTime.miSeconds = 0;
        lMessageData.mStartTimeMessageSend.mTime.mfFraction = 0.0f;

        lMessageData.mStartTimeMessageRecv.muVTable = KU_START_TIME_MESSAGE_VTABLE;
        lMessageData.mStartTimeMessageRecv.mTime.miSeconds = 0;
        lMessageData.mStartTimeMessageRecv.mTime.mfFraction = 0.0f;

        lMessageData.mReadyMessageSend.muVTable = KU_READY_MESSAGE_VTABLE;
        lMessageData.mReadyMessageRecv.muVTable = KU_READY_MESSAGE_VTABLE;
    }

    mStartTime.mTime.miSeconds = 0;
    mStartTime.mTime.mfFraction = 0.0f;

    mStatusEnterTime.miSeconds = 0;
    mStatusEnterTime.mfFraction = 0.0f;
    mMinTimeToSyncTime.miSeconds = 0;
    mMinTimeToSyncTime.mfFraction = 0.0f;
    mMaxTimeToSyncTime.miSeconds = 0;
    mMaxTimeToSyncTime.mfFraction = 0.0f;
    mTimeToWaitForStartTime.miSeconds = 0;
    mTimeToWaitForStartTime.mfFraction = 0.0f;
    mTimeToWaitForSilentClientReady.miSeconds = 0;
    mTimeToWaitForSilentClientReady.mfFraction = 0.0f;
    mTimeToWaitForCommunicatingClientReady.miSeconds = 0;
    mTimeToWaitForCommunicatingClientReady.mfFraction = 0.0f;
    mGapToLeaveBeforeStartTime.miSeconds = 0;
    mGapToLeaveBeforeStartTime.mfFraction = 0.0f;
}

StartTimeManager::EPlayerReadiness StartTimeManager::AreAllPlayersReadyTo(
    const TimerStatus* lpTimerStatus,
    const void*,
    const Time* lpTimeout)
{
    if (lpTimerStatus->mTime >= *lpTimeout)
        return E_TIMEOUT;

    s32 liPlayerID = -1;
    PlayerManager* lpPlayerManager = GetPlayerManager();

    while (lpPlayerManager->GetNextPlayerID(&liPlayerID, 1))
    {
        const PlayerManager::PlayerData* lpPlayerData = lpPlayerManager->FindActivePlayer(liPlayerID);
        if (!lpPlayerData)
            continue;

        NetworkPlayer* lpPlayer = lpPlayerData->GetPlayer();
        if (!lpPlayer)
            continue;

        if (lpPlayerManager->GetConnectionStatus(liPlayerID) != KI_CONNECTED_STATUS)
            return E_NOT_READY;

        if (!lpPlayer->mbHasConnectionFailed && !lpPlayer->mbNetworkPlayerPaused && !IsPlayerReady(liPlayerID))
            return E_NOT_READY;
    }

    return E_READY;
}
}
