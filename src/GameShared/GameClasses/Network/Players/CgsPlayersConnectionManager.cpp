#include "CgsPlayersConnectionManager.h"

namespace CgsNetwork
{
PlayersConnectionManager::PlayersConnectionManager()
{
    for (ConnectionRecord& lConnection : maConnections)
    {
        lConnection.muReadyVTable0 = 0x820CF5E0;
        lConnection.muReadyVTable1 = 0x820CF5E0;
        lConnection.muConnectionState = 0;
        lConnection.mfConnectionTime = 0.0f;
        lConnection.muTimerVTable0 = 0x820CF60C;
        lConnection.muTimerVTable1 = 0x820CF60C;
    }
}
}
