#ifndef BRN_VEHICLE_SOA_DATA_H
#define BRN_VEHICLE_SOA_DATA_H

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"

namespace BrnTraffic
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827xxxxx.
// Structure-of-arrays traffic vehicle data; Construct zeroes every bit set.
// Member names/types per burnout.wiki (World Module / Traffic Entity Module ->
// BrnTraffic::VehicleSoaData): eight FastBitArray<601> sets, each 0x50 bytes,
// covering up to 601 active traffic vehicles. Layout (8 x 0x50 = 0x280) verified
// against the X360 pseudocode's quadword clears.
struct VehicleSoaData
{
    static const u32 KU_MAX_VEHICLES = 601;

    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mAliveVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mVehiclesWithEntities;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mCollidableVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mArticulatedVehicles;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mVehiclesRenderedLastFrame;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehiclesFarFromPlayer;
    CgsContainers::FastBitArray<KU_MAX_VEHICLES> mPhysicalVehiclesTryingToRecover;

    void Construct();
};
}

#endif
