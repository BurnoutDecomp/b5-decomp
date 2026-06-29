// ===================================================================================
// BrnNetwork::NetworkPlayerStatsResults  -- owning header
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStatsResults.h
//
// The fixed-capacity cache of downloaded per-player online stats records that the
// NetworkPlayerStatsManager embeds as `mPlayerStatsCache`. It owns a NetworkPlayerStats
// record array, a live-count, a back-pointer to the network manager, and a by-value
// stats debug component.
//
// SHAPE (member names/types/order + the method set) is from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Network/Managers/BrnNetworkPlayerStatsResults.h,
// BrnNetworkPlayerStatsResults.h:46..115), gated against the X360 binary: the manager's
// GetPlayerStatsByName (@0x825526C8) reaches this object at the manager's +0x578 and
// tail-calls its GetPlayerStats(const char*).
//
// NOTE on member type: the DWARF spells the NetworkPlayerID-keyed GetPlayerStats overload
// with a drifting nested scope (RoadRulesRecvData / GuiEventNetworkLaunching /
// AggressiveMoveData :: NetworkPlayerID) across DIE copies. On X360 this is the single
// committed top-level typedef BrnNetwork::NetworkPlayerID (== s32) from BrnNetworkSharedIO.h.
//
// No fixed-byte sizeof/offset static_assert is emitted: the PC gate targets x64, where the
// embedded pointers / debug-component / NetworkPlayerStats records widen and shift the
// absolute byte offsets relative to the X360 32-bit layout. The by-name member walk the
// compiler emits is identical.
#pragma once

#include "types.hpp"
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"          // NetworkPlayerStats
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"             // NetworkPlayerID
#include "GameSource/Network/Debug Components/BrnNetworkStatsDebugComponent.h" // StatsDebugComponent

namespace BrnNetwork
{
    class BrnNetworkManager;   // back-pointer member only (BrnNetworkManager.h)

    class NetworkPlayerStatsResults
    {
    public:
        // DWARF :51..103 -- lifecycle + lookup surface. All declared-only here; the bodies
        // live in this class's own TUs (BrnNetworkPlayerStatsResults.cpp).
        void Construct(BrnNetworkManager* lpNetworkManager);
        bool Prepare();
        bool Release();
        void Destruct();
        void Clear();

        // The two stat-record lookups the manager drives. The const-char* overload is the
        // direct callee of NetworkPlayerStatsManager::GetPlayerStatsByName.
        NetworkPlayerStats* GetPlayerStats(const char* lpcName);
        NetworkPlayerStats* GetPlayerStats(NetworkPlayerID lPlayerID);
        NetworkPlayerStats* GetLocalPlayerStats();
        NetworkPlayerStats* InsertPlayerStats(const NetworkPlayerStats& lStats);

    private:
        NetworkPlayerStats* FindReplaceableRecordSet();
        NetworkPlayerStats* FindFirstRecordWithState(NetworkPlayerStats::EStatsStatus leStatus);
        bool                IsPlayerInTheGame(const char* lpcName);
        s32                 GetLength();

        // DWARF :106 -- the record-cache capacity.
        static const s32 KI_CACHE_SIZE = 32;

        // ---- data layout (DWARF :108..115) -------------------------------------
        NetworkPlayerStats maPlayerStatsCache[KI_CACHE_SIZE]; // +0x000  the record ring
        s32                miCacheSize;                       // live record count
        BrnNetworkManager* mpNetworkManager;                  // back-pointer
        StatsDebugComponent mStatsDebugComponent;             // by-value debug component
    };
}
