#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameShared\\GameClasses\\Network\\Players\\CgsPlayersConnectionManager.cpp"

// CgsNetwork::PlayersConnectionManager::GetEntry -- the two connection-entry lookups. Both
// scan the seven entries for the one whose player id matches; the const one asserts with the
// id streamed into the message on a miss, the other asserts on the null result.

namespace CgsNetwork
{

const PlayersConnectionManager::ConnectionDataEntry*
PlayersConnectionManager::GetEntry(NetworkPlayerID lPlayerID) const
{
    CGS_ASSERT_AT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 1138);

    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        if (maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID == lPlayerID)
        {
            return &maConnectionDataEntry[liIndex];
        }
    }

    CgsDev::Assert::BeginAssert();
    CgsDev::StrStream lStream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
    lStream << "Could not find player " << lPlayerID << " conenction entry\n";
    CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer, CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 1148));
    CgsDev::Assert::EndAssert();
    return 0;
}

PlayersConnectionManager::ConnectionDataEntry*
PlayersConnectionManager::GetEntry(NetworkPlayerID lPlayerID)
{
    ConnectionDataEntry* lpEntry = 0;
    CGS_ASSERT_AT(lPlayerID != K_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 1167);
    for (s32 liIndex = 0; liIndex < KI_MAX_CONNECTION_ENTRIES; ++liIndex)
    {
        if (maConnectionDataEntry[liIndex].mPlayerConnectionData.mPlayerID == lPlayerID)
        {
            lpEntry = &maConnectionDataEntry[liIndex];
            break;
        }
    }

    CGS_ASSERT_AT(lpEntry, "lpEntry", CGS_ARTIST_FILE, 1178);
    return lpEntry;
}

}
