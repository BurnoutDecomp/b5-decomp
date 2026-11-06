#pragma once

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Containers/CgsSet.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include <cstddef>

namespace BrnReplays
{
    static const u32 KU_MAX_BONUS_STREAMED_ASSETS = 2;

    struct TrafficSerialiserFrame
    {
        typedef ::Set<u16, BrnTraffic::KU_MAX_ACTIVE_HULLS> ActiveHullSet;

        ActiveHullSet mActiveHullsForLocalPlayer;                     // +0x00
        u16 muLocalPlayerHull;                                        // +0x94
        u8 muNumBonusAssets;                                          // +0x96
        u8 maBonusAssets[KU_MAX_BONUS_STREAMED_ASSETS];                                          // +0x97
        CgsContainers::FastBitArray<BrnTraffic::KU_MAX_TOTAL_TRAFFIC> mVehicleUpdateBits; // +0xA0
        CgsContainers::FastBitArray<BrnTraffic::KU_MAX_PHYSICAL_TRAFFIC_VEHICLES> mPhysicsBits; // +0xF0

        TrafficSerialiserFrame* Clear();                              // ARTIST 0x826533D8
    };

    CGS_STATIC_ASSERT((offsetof(TrafficSerialiserFrame, mActiveHullsForLocalPlayer) == 0),
                      "ARTIST traffic replay active hull set");
    CGS_STATIC_ASSERT((offsetof(TrafficSerialiserFrame, muNumBonusAssets) == 0x96 &&
                       offsetof(TrafficSerialiserFrame, maBonusAssets) == 0x97),
                      "ARTIST traffic replay bonus asset fields");
    CGS_STATIC_ASSERT((offsetof(TrafficSerialiserFrame, muLocalPlayerHull) == 0x94),
                      "ARTIST traffic replay player hull");
    CGS_STATIC_ASSERT((offsetof(TrafficSerialiserFrame, mVehicleUpdateBits) == 0xA0),
                      "ARTIST traffic replay vehicle bits");
    CGS_STATIC_ASSERT((offsetof(TrafficSerialiserFrame, mPhysicsBits) == 0xF0),
                      "ARTIST traffic replay physics bits");
    CGS_STATIC_ASSERT((sizeof(TrafficSerialiserFrame) == 0xF8),
                      "ARTIST traffic replay frame header extent");
}
