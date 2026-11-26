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
//   GetPlayerStats(NetworkPlayerID) -- linear player-id lookup (the unnamed console copy the
//     manager's UpdateLocalPlayersStat calls).
//   Construct / Release / Destruct / GetLocalPlayerStats -- no standalone console copy; inlined
//     into the manager's own lifecycle functions and GetLocalPlayerStats.
//   InsertPlayerStats -- store a finished record over the player's old one (or a free /
//     replaceable slot) and re-register it with the stats debug component.
//   Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX.
// ===================================================================================
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsResults.h"
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnServerInterface.h"
#include "GameSource/Network/Debug Components/BrnNetworkStatsDebugComponent.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // ::LobbyNameCmp (the vendor name compare)
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStatsResults.cpp"
// The assert locations are ARTIST's (CGS_ARTIST_FILE and the original lines).

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
        CGS_ASSERT_AT(miCacheSize > 0,
            "miCacheSize > 0",
                "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStatsResults.cpp", 237 /* ARTIST 0x82552D44 */);


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

        return 0;
    }

    // @ 0x82552C20 -- reset the cache to empty and prime the stats debug component.
    bool
    NetworkPlayerStatsResults::Prepare()
    {
        miCacheSize = 0;

        CGS_ASSERT_AT(mpNetworkManager,
            "mpNetworkManager", CGS_ARTIST_FILE, 77 /* ARTIST 0x82552C5C */);
        CGS_ASSERT_AT(mpNetworkManager->GetServerInterface(),
            "mpNetworkManager->GetServerInterface()",
                CGS_ARTIST_FILE, 78 /* ARTIST 0x82552C84 */);


        CGS_ASSERT_AT(
            mStatsDebugComponent.Prepare(mpNetworkManager->GetServerInterface(),
                                         mpNetworkManager->GetStatsManager()),
            "mStatsDebugComponent.Prepare( mpNetworkManager->GetServerInterface(), mpNetworkManager->GetStatsManager() )",
                CGS_ARTIST_FILE, 80 /* ARTIST 0x82552CC4 */);


        return true;
    }
    // Latch the network manager, build the debug component and start empty.
    void
    NetworkPlayerStatsResults::Construct(BrnNetworkManager* lpNetworkManager)
    {
        mpNetworkManager = lpNetworkManager;
        mStatsDebugComponent.Construct();
        miCacheSize = 0;
    }

    bool
    NetworkPlayerStatsResults::Release()
    {
        miCacheSize = 0;
        // Inlined into NetworkPlayerStatsManager::Release (ARTIST 0x82559F68, no out-of-line copy).
        CGS_ASSERT_AT(mStatsDebugComponent.Release(), "mStatsDebugComponent.Release()",
                      CGS_ARTIST_FILE, 100 /* ARTIST 0x82559F68 */);
        return true;
    }

    void
    NetworkPlayerStatsResults::Destruct()
    {
        miCacheSize = 0;
        mStatsDebugComponent.Destruct();
    }

    // The cached record belonging to the local player, if any.
    NetworkPlayerStats*
    NetworkPlayerStatsResults::GetLocalPlayerStats()
    {
        for (s32 luCacheLoopCounter = 0; luCacheLoopCounter < miCacheSize; ++luCacheLoopCounter)
        {
            if (maPlayerStatsCache[luCacheLoopCounter].IsLocalPlayer())
            {
                return &maPlayerStatsCache[luCacheLoopCounter];
            }
        }
        return 0;
    }

    // Linear player-id lookup into the record cache.
    NetworkPlayerStats*
    NetworkPlayerStatsResults::GetPlayerStats(NetworkPlayerID lPlayerID)
    {
        CGS_ASSERT_AT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID", CGS_ARTIST_FILE, 176 /* ARTIST 0x82546D68 */);

        for (s32 luCacheLoopCounter = 0; luCacheLoopCounter < miCacheSize; ++luCacheLoopCounter)
        {
            if (maPlayerStatsCache[luCacheLoopCounter].GetPlayerID() == lPlayerID)
            {
                return &maPlayerStatsCache[luCacheLoopCounter];
            }
        }
        return 0;
    }

    // Store a finished record: over the player's existing record (found by id, then by name), or
    // into a free / replaceable slot. The debug component drops the old record's menu group and
    // registers the new one.
    NetworkPlayerStats*
    NetworkPlayerStatsResults::InsertPlayerStats(const NetworkPlayerStats& lNewStats)
    {
        CGS_ASSERT_AT(lNewStats.GetStatus() != NetworkPlayerStats::E_STATS_AGE_UNPREPARED,
            "lNewStats.GetStatus() != NetworkPlayerStats::E_STATS_AGE_UNPREPARED",
                CGS_ARTIST_FILE, 295 /* ARTIST 0x82552E40 */);


        NetworkPlayerStats* lpPlaceToInsert = 0;
        if (lNewStats.GetPlayerID() != CgsNetwork::K_INVALID_PLAYER_ID)
        {
            lpPlaceToInsert = GetPlayerStats(lNewStats.GetPlayerID());
        }
        if (lpPlaceToInsert == 0)
        {
            lpPlaceToInsert = GetPlayerStats(lNewStats.GetName());
        }

        if (lpPlaceToInsert == 0)
        {
            lpPlaceToInsert = FindReplaceableRecordSet();
        }
        else
        {
            mStatsDebugComponent.RemoveStats(lpPlaceToInsert);
        }

        CGS_ASSERT_AT(lpPlaceToInsert != 0,
            "lpPlaceToInsert != NULL",
                CGS_ARTIST_FILE, 324 /* ARTIST 0x82552EB8 */);

        *lpPlaceToInsert = lNewStats;
        mStatsDebugComponent.AddStats(lpPlaceToInsert);
        return lpPlaceToInsert;
    }
}
