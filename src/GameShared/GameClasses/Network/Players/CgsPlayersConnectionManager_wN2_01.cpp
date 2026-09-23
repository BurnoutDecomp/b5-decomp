#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
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
    if (lpConnApi == nullptr)
    {
        return;
    }

    const DirtySock::ConnApiClientListT* lpClientList = ConnApiGetClientList(lpConnApi);
    if (lpClientList == nullptr)
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
    NetworkPlayerID lLocalPlayerID;
    mpPlayerManager->GetNextLocalPlayerID(&lLocalPlayerID);

    CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");
    CGS_ASSERT(lPlayerID != lLocalPlayerID, "lPlayerID != lLocalPlayerID");

    const ConnectionDataEntry* lpEntry = GetEntry(lPlayerID);
    CGS_ASSERT(lpEntry, "lpEntry");
    return lpEntry->mPlayerConnectionData.meConnectionStatus;
}

// ---- HavePlayersFailedToConnect ------------------------------------------------------
// True when either side of the (lPlayerID1, lPlayerID2) pair reports E_CONNECTION_FAILURE:
// our own entry for the other player when one of them is us, else what each player last
// told us about the other.
bool PlayersConnectionManager::HavePlayersFailedToConnect(NetworkPlayerID lPlayerID1,
                                                          NetworkPlayerID lPlayerID2) const
{
    CGS_ASSERT(lPlayerID1 != K_INVALID_PLAYER_ID, "lPlayerID1 != K_INVALID_PLAYER_ID");
    CGS_ASSERT(lPlayerID2 != K_INVALID_PLAYER_ID, "lPlayerID2 != K_INVALID_PLAYER_ID");

    NetworkPlayerID lLocalPlayerID;
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
        CGS_ASSERT(lpPlayer1Entry, "lpPlayer1Entry");
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
    CGS_ASSERT(lpPlayer2Entry, "lpPlayer2Entry");
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
            *CgsDev::Log::gpDebugPrint
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
                        *CgsDev::Log::gpDebugPrint
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
                *CgsDev::Log::gpDebugPrint
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
