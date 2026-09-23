#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork::PlayersConnectionManager::GetEntry -- the two connection-entry lookups. Both
// scan the seven entries for the one whose player id matches; the const one asserts with the
// id streamed into the message on a miss, the other asserts on the null result.

namespace CgsNetwork
{

const PlayersConnectionManager::ConnectionDataEntry*
PlayersConnectionManager::GetEntry(NetworkPlayerID lPlayerID) const
{
    CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        if (maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID == lPlayerID)
        {
            return &maConnectionDataEntry[liIndex];
        }
    }

    // The console streams the player id between these two literals.
    CGS_ASSERT(false, "Could not find player  conenction entry\n");
    return nullptr;
}

PlayersConnectionManager::ConnectionDataEntry*
PlayersConnectionManager::GetEntry(NetworkPlayerID lPlayerID)
{
    CGS_ASSERT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

    ConnectionDataEntry* lpEntry = nullptr;
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        if (maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID == lPlayerID)
        {
            lpEntry = &maConnectionDataEntry[liIndex];
            break;
        }
    }

    CGS_ASSERT(lpEntry, "lpEntry");
    return lpEntry;
}

}
