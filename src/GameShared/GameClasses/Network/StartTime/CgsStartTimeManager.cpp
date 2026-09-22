#include "types.hpp"
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

namespace CgsNetwork
{
// The console ctor only seeds the embedded members: the start-time and ready message
// objects of every slot and the time members (CgsSystem::Time's own ctor zeroes them).
StartTimeManager::StartTimeManager()
{
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
}
