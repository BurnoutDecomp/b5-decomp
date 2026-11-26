#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"
#include "BrnCommonTypes.h"                                   // s32 / f32
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"            // CGS_ASSERT / CgsDev::Assert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"       // CgsCore::StrnCpy (the name copy)
#include "GameSource/GameState/BrnCgsPlayerName.h"            // CgsNetwork::KI_USERNAME_LENGTH
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStats.cpp"
#include <cstdlib>   // atoi
#include <cstring>   // strlen / strncpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::NetworkPlayerStats::operator=  @ 0x82355C50
//
// Plain member-wise copy assignment. The X360 build lowers this to three short
// word/byte copy loops (the two 12x int arrays, then a 16-byte run plus tail dwords
// over the timestamp + name + id) followed by individual stores of the trailing
// status/flag block. That is just every data member copied in declaration order;
// reconstructed here as the logical member-wise copy.
BrnNetwork::NetworkPlayerStats&
BrnNetwork::NetworkPlayerStats::operator=(const NetworkPlayerStats& lOther)
{
    // +0  : maiValues[12]            (X360: 12-iteration dword copy loop)
    for (s32 li = 0; li < EStatsValue::E_STATS_VALUE_COUNT; ++li)
    {
        maiValues[li] = lOther.maiValues[li];
    }

    // +48 : maeStatType[12]          (X360: second 12-iteration dword copy loop)
    for (s32 li = 0; li < EStatsValue::E_STATS_VALUE_COUNT; ++li)
    {
        maeStatType[li] = lOther.maeStatType[li];
    }

    // +96 : mTimeStamp (CgsSystem::Time)  (X360: two dword stores at +96/+100;
    //       logically Time's copy-assign)
    mTimeStamp = lOther.mTimeStamp;

    // +104: macName[16]              (a 16-byte run)
    for (s32 li = 0; li < static_cast<s32>(sizeof(macName)); ++li)
    {
        macName[li] = lOther.macName[li];
    }

    // +124: trailing scalar block (id / status / flags)
    mNetworkPlayerID = lOther.mNetworkPlayerID;   // +124
    meStatsStatus    = lOther.meStatsStatus;      // +128
    mbIsCalulated    = lOther.mbIsCalulated;       // +132
    mbIsLocalPlayer  = lOther.mbIsLocalPlayer;     // +133

    return *this;
}

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStats.cpp".

namespace BrnNetwork
{
    // Empty record: no name, every stat 0 and typed as a plain number, a zero timestamp, not prepared,
    // not calculated, not the local player, no player id.
    void NetworkPlayerStats::Clear()
    {
        macName[0] = 0;

        for (EStatsValue leValue = E_STATS_VALUE_START; leValue < E_STATS_VALUE_COUNT; leValue++)
        {
            maiValues[leValue]   = 0;
            maeStatType[leValue] = E_STAT_TYPE_NUMBER;
        }

        mTimeStamp       = 0.0f;
        meStatsStatus    = E_STATS_AGE_UNPREPARED;
        mbIsCalulated    = false;
        mbIsLocalPlayer  = false;
        mNetworkPlayerID = -1;
    }

    // A fresh record is an empty one.
    void NetworkPlayerStats::Construct()
    {
        Clear();
    }

    // Start a record for one player: empty it, copy the name (bounded to the user-name length),
    // stamp it, mark it current and remember who it belongs to.
    bool NetworkPlayerStats::Prepare(const char* lpcName, Time lTimeStamp, bool lbIsLocalPlayer,
                                     NetworkPlayerID lPlayerID)
    {
        Clear();

        CGS_ASSERT_AT(std::strlen(lpcName) < static_cast<u32>(CgsNetwork::KI_USERNAME_LENGTH),
                      "strlen(lpcName) < static_cast<uint32_t>(CgsNetwork::KI_USERNAME_LENGTH)",
                      CGS_ARTIST_FILE, 99);

        // CgsCore::StrnCpy (ARTIST 0x82546878 asserts CgsStringUtils.h:55).
        CgsCore::StrnCpy(macName, lpcName, sizeof(macName));

        mTimeStamp       = lTimeStamp;
        meStatsStatus    = E_STATS_AGE_INVALID;
        mNetworkPlayerID = lPlayerID;
        mbIsLocalPlayer  = lbIsLocalPlayer;
        return true;
    }

    // Parse one downloaded stat string into its value slot and record how it is displayed.
    void NetworkPlayerStats::SetStat(EStatsValue leValue, const char* lpcStat, EStatType leType)
    {
        CGS_ASSERT_AT(leValue >= E_STATS_VALUE_START,
            "leValue >= E_STATS_VALUE_START", CGS_ARTIST_FILE, 195 /* ARTIST 0x825469DC */);

        CGS_ASSERT_AT(leValue < E_STATS_VALUE_COUNT,
            "leValue < E_STATS_VALUE_COUNT", CGS_ARTIST_FILE, 196 /* ARTIST 0x82546A00 */);

        CGS_ASSERT_AT(lpcStat != 0,
            "lpcStat", CGS_ARTIST_FILE, 197 /* ARTIST 0x82546A24 */);

        maiValues[leValue]   = std::atoi(lpcStat);
        maeStatType[leValue] = leType;
    }
}

// FOLDED FROM BrnNetworkPlayerStats_wN3_00.cpp
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

// Asserts pass __FILE__; ARTIST's reads "..\\..\\..\\GameSource\\Network/Managers/BrnNetworkPlayerStats.cpp".

namespace BrnNetwork
{
    // ------------------------------------------------------------ GetStatAsInt
    const s32 NetworkPlayerStats::GetStatAsInt(EStatsValue leValue) const
    {
        CGS_ASSERT_AT(leValue >= E_STATS_VALUE_START,
            "leValue >= E_STATS_VALUE_START", CGS_ARTIST_FILE, 154 /* ARTIST 0x825468EC */);

        CGS_ASSERT_AT(leValue < E_STATS_VALUE_COUNT,
            "leValue < E_STATS_VALUE_COUNT", CGS_ARTIST_FILE, 155 /* ARTIST 0x82546910 */);


        return maiValues[leValue];
    }

    // ------------------------------------------------------------ GetStatAsString
    void NetworkPlayerStats::GetStatAsString(EStatsValue leValue, char* lpcBuffer, s32 liSize) const
    {
        CGS_ASSERT_AT(liSize >= KI_MININUM_CHAR_BUFFER_SIZE,
            "liSize >= KI_MININUM_CHAR_BUFFER_SIZE", CGS_ARTIST_FILE, 174 /* ARTIST 0x82546964 */);


        CgsCore::SPrintf(lpcBuffer, liSize, "%i", GetStatAsInt(leValue));
    }

    // ------------------------------------------------------------ GetStatType
    NetworkPlayerStats::EStatType NetworkPlayerStats::GetStatType(EStatsValue leValue) const
    {
        CGS_ASSERT_AT(leValue >= E_STATS_VALUE_START,
            "leValue >= E_STATS_VALUE_START", CGS_ARTIST_FILE, 218 /* ARTIST 0x82546A8C */);

        CGS_ASSERT_AT(leValue < E_STATS_VALUE_COUNT,
            "leValue < E_STATS_VALUE_COUNT", CGS_ARTIST_FILE, 219 /* ARTIST 0x82546AB0 */);


        return maeStatType[leValue];
    }
}
