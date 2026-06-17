#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"
#include "BrnCommonTypes.h"                                   // s32 / f32
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"   // BrnNetwork::NetworkPlayerID
#include "GameShared/GameClasses/System/Timer/CgsTime.h"      // CgsSystem::Time

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

    // +104: macName[20]              (X360: 16-byte run + tail dword at +120)
    for (s32 li = 0; li < 20; ++li)
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
