#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"
#include "BrnCommonTypes.h"                                   // s32 / f32
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT / CgsDev::Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (the name-copy assert)
#include "GameSource/GameState/BrnCgsPlayerName.h"            // CgsNetwork::KI_USERNAME_LENGTH

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

        CGS_ASSERT(std::strlen(lpcName) < static_cast<u32>(CgsNetwork::KI_USERNAME_LENGTH),
                   "strlen(lpcName) < static_cast<uint32_t>(CgsNetwork::KI_USERNAME_LENGTH)");

        // The bounded string copy re-checks the length and names the offending string.
        if (!(std::strlen(lpcName) < sizeof(macName)))
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "String too long: " << (lpcName != nullptr ? lpcName : "<NULLSTRING>");
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        std::strncpy(macName, lpcName, sizeof(macName));

        mTimeStamp       = lTimeStamp;
        meStatsStatus    = E_STATS_AGE_INVALID;
        mNetworkPlayerID = lPlayerID;
        mbIsLocalPlayer  = lbIsLocalPlayer;
        return true;
    }

    // Parse one downloaded stat string into its value slot and record how it is displayed.
    void NetworkPlayerStats::SetStat(EStatsValue leValue, const char* lpcStat, EStatType leType)
    {
        CGS_ASSERT(leValue >= E_STATS_VALUE_START, "leValue >= E_STATS_VALUE_START");
        CGS_ASSERT(leValue < E_STATS_VALUE_COUNT, "leValue < E_STATS_VALUE_COUNT");
        CGS_ASSERT(lpcStat != nullptr, "lpcStat");

        maiValues[leValue]   = std::atoi(lpcStat);
        maeStatType[leValue] = leType;
    }
}
