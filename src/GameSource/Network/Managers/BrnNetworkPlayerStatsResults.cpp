// ===================================================================================
// BrnNetwork::NetworkPlayerStatsResults  -- implementation
//   class:BrnNetwork::NetworkPlayerStatsResults
//
// The fixed-capacity cache of downloaded per-player online stats records embedded by the
// NetworkPlayerStatsManager. This TU homes:
//   FindReplaceableRecordSet @ 0x82552CF8 -- pick the slot for a new record (append while
//     not full, otherwise evict the oldest record whose player has left the game).
//   GetPlayerStats(const char*) @ 0x82546CF8 -- linear name lookup into the cache.
//   Prepare @ 0x82552C20 -- reset the cache and prime the stats debug component.
//   Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// ===================================================================================
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsResults.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnServerInterface.h"
#include "GameSource/Network/Debug Components/BrnNetworkStatsDebugComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// X360 LobbyNameCmp -- compares two 16-byte player-name strings (returns 0 when they
// name the same player, <0/>0 for ordering). It is a free function with no committed
// header home yet; declared here at global scope so this TU links against the one X360
// definition (same file-local idiom as BrnNetworkBuddyManagerBase.cpp:23 /
// BrnChallengeHighScoreEntry.cpp).
int LobbyNameCmp(const char* lpcNameA, const char* lpcNameB);

namespace BrnNetwork
{
    // @ 0x82552CF8 -- choose the record slot for a new player-stats entry.
    NetworkPlayerStats*
    NetworkPlayerStatsResults::FindReplaceableRecordSet()
    {
        // Not full: append into the next free slot and grow the live count.
        if (miCacheSize < KI_CACHE_SIZE)
        {
            NetworkPlayerStats* lpSlot = &maPlayerStatsCache[miCacheSize];
            ++miCacheSize;
            return lpSlot;
        }

        // Full: evict the oldest (smallest-timestamp) record whose player is no
        // longer in the game.
        CGS_ASSERT(miCacheSize > 0, "miCacheSize > 0");

        Time lSmallestTime  = maPlayerStatsCache[0].GetTimeStamp();
        s32  liSmallestIndex = 0;

        for (s32 li = 1; li < miCacheSize; ++li)
        {
            if (maPlayerStatsCache[li].GetTimeStamp() < lSmallestTime
                && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsPlayerInGame(
                       maPlayerStatsCache[li].GetName()))
            {
                lSmallestTime   = maPlayerStatsCache[li].GetTimeStamp();
                liSmallestIndex = li;
            }
        }

        return &maPlayerStatsCache[liSmallestIndex];
    }

    // @ 0x82546CF8 -- linear name lookup into the record cache.
    NetworkPlayerStats*
    NetworkPlayerStatsResults::GetPlayerStats(const char* lpcName)
    {
        for (s32 li = 0; li < miCacheSize; ++li)
        {
            if (LobbyNameCmp(maPlayerStatsCache[li].macName, lpcName) == 0)
            {
                return &maPlayerStatsCache[li];
            }
        }

        return nullptr;
    }

    // @ 0x82552C20 -- reset the cache to empty and prime the stats debug component.
    bool
    NetworkPlayerStatsResults::Prepare()
    {
        miCacheSize = 0;

        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");

        CGS_ASSERT(
            mStatsDebugComponent.Prepare(mpNetworkManager->GetServerInterface(),
                                         mpNetworkManager->GetStatsManager()),
            "mStatsDebugComponent.Prepare( mpNetworkManager->GetServerInterface(), mpNetworkManager->GetStatsManager() )");

        return true;
    }
}
