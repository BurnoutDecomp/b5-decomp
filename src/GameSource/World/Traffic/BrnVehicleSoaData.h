#ifndef BRN_VEHICLE_SOA_DATA_H
#define BRN_VEHICLE_SOA_DATA_H

#include "types.hpp"

namespace BrnTraffic
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827xxxxx.
// Structure-of-arrays traffic vehicle data; Construct zeroes every bit set.
// Member names/types per burnout.wiki (World Module / Traffic Entity Module ->
// BrnTraffic::VehicleSoaData): eight FastBitArray<601> sets, each 0x50 bytes,
// covering up to 601 active traffic vehicles. Layout (8 x 0x50 = 0x280) verified
// against the X360 pseudocode's quadword clears.
class VehicleSoaData
{
public:
    VehicleSoaData* Construct();

private:
    // FastBitArray<601> — 601 vehicle bits in a 0x50-byte block (10 quadwords).
    typedef u64 FastBitArray601[10];

    FastBitArray601 mAliveVehicles;                   // 0x000
    FastBitArray601 mVehiclesWithEntities;            // 0x050
    FastBitArray601 mCollidableVehicles;              // 0x0A0
    FastBitArray601 mPhysicalVehicles;                // 0x0F0
    FastBitArray601 mArticulatedVehicles;             // 0x140
    FastBitArray601 mVehiclesRenderedLastFrame;       // 0x190
    FastBitArray601 mPhysicalVehiclesFarFromPlayer;   // 0x1E0
    FastBitArray601 mPhysicalVehiclesTryingToRecover; // 0x230
};
}

#endif
