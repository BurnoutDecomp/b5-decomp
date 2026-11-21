// ===================================================================================
// BrnNetwork::NetworkPlayerStats -- partfile: the three per-stat readers.
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkPlayerStats_wN3_00.cpp
//
// Reconstructed from the console image (assembly listing authoritative). Their
// consumer is GuiNetworkPlayerStats::FormatNetworkStats (the online player-stats panel).
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

static const char KAC_ARTIST_ASSERT_FILE[] =
    "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStats.cpp";

namespace BrnNetwork
{
    // ------------------------------------------------------------ GetStatAsInt
    const s32 NetworkPlayerStats::GetStatAsInt(EStatsValue leValue) const
    {
        CGS_ASSERT(leValue >= E_STATS_VALUE_START,
            CgsDev::Assert::MessageAt(
                "leValue >= E_STATS_VALUE_START", KAC_ARTIST_ASSERT_FILE, 154 /* ARTIST 0x825468EC */));
        CGS_ASSERT(leValue < E_STATS_VALUE_COUNT,
            CgsDev::Assert::MessageAt(
                "leValue < E_STATS_VALUE_COUNT", KAC_ARTIST_ASSERT_FILE, 155 /* ARTIST 0x82546910 */));

        return maiValues[leValue];
    }

    // ------------------------------------------------------------ GetStatAsString
    void NetworkPlayerStats::GetStatAsString(EStatsValue leValue, char* lpcBuffer, s32 liSize) const
    {
        CGS_ASSERT(liSize >= KI_MININUM_CHAR_BUFFER_SIZE,
            CgsDev::Assert::MessageAt(
                "liSize >= KI_MININUM_CHAR_BUFFER_SIZE", KAC_ARTIST_ASSERT_FILE, 174 /* ARTIST 0x82546964 */));

        CgsCore::SPrintf(lpcBuffer, liSize, "%i", GetStatAsInt(leValue));
    }

    // ------------------------------------------------------------ GetStatType
    NetworkPlayerStats::EStatType NetworkPlayerStats::GetStatType(EStatsValue leValue) const
    {
        CGS_ASSERT(leValue >= E_STATS_VALUE_START,
            CgsDev::Assert::MessageAt(
                "leValue >= E_STATS_VALUE_START", KAC_ARTIST_ASSERT_FILE, 218 /* ARTIST 0x82546A8C */));
        CGS_ASSERT(leValue < E_STATS_VALUE_COUNT,
            CgsDev::Assert::MessageAt(
                "leValue < E_STATS_VALUE_COUNT", KAC_ARTIST_ASSERT_FILE, 219 /* ARTIST 0x82546AB0 */));

        return maeStatType[leValue];
    }
}
