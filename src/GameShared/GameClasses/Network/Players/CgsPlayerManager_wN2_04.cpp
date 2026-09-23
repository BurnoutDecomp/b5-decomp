#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"   // GetConnectionData
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "netgamelink.h"                                                // NetGameLinkStatus

// CgsNetwork::PlayerManager -- the bandwidth totals the debug HUD shows: one per-second rate
// from every remote player's game link, summed.

namespace CgsNetwork
{

// ---- GetTotalBytesSent ---------------------------------------------------------------
// Bytes per second handed to the transport (payload plus link framing).
u32 PlayerManager::GetTotalBytesSent()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        const NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        u32 luRate = 0;
        if (lpNetworkPlayer->GetConnectionData().IsValid())
        {
            const NetGameLinkStatT* lpStat = NetGameLinkStatus(lpNetworkPlayer->GetConnectionData().mpNetGameLink);
            if (lpStat != nullptr)
            {
                luRate = static_cast<u32>(lpStat->outrps);
            }
        }
        luTotal += luRate;
    }
    return luTotal;
}

// ---- GetTotalBytesSentToDirtySock ----------------------------------------------------
// Game payload bytes per second.
u32 PlayerManager::GetTotalBytesSentToDirtySock()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        const NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        u32 luRate = 0;
        if (lpNetworkPlayer->GetConnectionData().IsValid())
        {
            const NetGameLinkStatT* lpStat = NetGameLinkStatus(lpNetworkPlayer->GetConnectionData().mpNetGameLink);
            if (lpStat != nullptr)
            {
                luRate = static_cast<u32>(lpStat->outbps);
            }
        }
        luTotal += luRate;
    }
    return luTotal;
}

// ---- GetTotalBytesSentWithOverhead ---------------------------------------------------
// Bytes per second on the wire.
u32 PlayerManager::GetTotalBytesSentWithOverhead()
{
    u32 luTotal = 0;
    NetworkPlayerID lPlayerID = K_INVALID_PLAYER_ID;
    while (GetNextRemotePlayerID(&lPlayerID))
    {
        const NetworkPlayer* lpNetworkPlayer = GetPlayerByID(lPlayerID);
        CGS_ASSERT(lpNetworkPlayer, "lpNetworkPlayer");

        u32 luRate = 0;
        if (lpNetworkPlayer->GetConnectionData().IsValid())
        {
            const NetGameLinkStatT* lpStat = NetGameLinkStatus(lpNetworkPlayer->GetConnectionData().mpNetGameLink);
            if (lpStat != nullptr)
            {
                luRate = static_cast<u32>(lpStat->outnps);
            }
        }
        luTotal += luRate;
    }
    return luTotal;
}

}
