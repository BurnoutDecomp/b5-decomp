// ===================================================================================
// BrnNetwork::NetworkPlayerStats -- partfile: the three per-stat readers.
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStats_wN3_00.cpp
//
// Reconstructed from the console image (assembly listing authoritative). Their
// consumer is GuiNetworkPlayerStats::FormatNetworkStats (the online player-stats panel).
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

namespace BrnNetwork
{
    // ------------------------------------------------------------ GetStatAsInt
    const s32 NetworkPlayerStats::GetStatAsInt(EStatsValue leValue) const
    {
        CGS_ASSERT(leValue >= E_STATS_VALUE_START, "leValue >= E_STATS_VALUE_START");
        CGS_ASSERT(leValue < E_STATS_VALUE_COUNT, "leValue < E_STATS_VALUE_COUNT");

        return maiValues[leValue];
    }

    // ------------------------------------------------------------ GetStatAsString
    void NetworkPlayerStats::GetStatAsString(EStatsValue leValue, char* lpcBuffer, s32 liSize) const
    {
        CGS_ASSERT(liSize >= KI_MININUM_CHAR_BUFFER_SIZE, "liSize >= KI_MININUM_CHAR_BUFFER_SIZE");

        CgsCore::SPrintf(lpcBuffer, liSize, "%i", GetStatAsInt(leValue));
    }

    // ------------------------------------------------------------ GetStatType
    NetworkPlayerStats::EStatType NetworkPlayerStats::GetStatType(EStatsValue leValue) const
    {
        CGS_ASSERT(leValue >= E_STATS_VALUE_START, "leValue >= E_STATS_VALUE_START");
        CGS_ASSERT(leValue < E_STATS_VALUE_COUNT, "leValue < E_STATS_VALUE_COUNT");

        return maeStatType[leValue];
    }
}
