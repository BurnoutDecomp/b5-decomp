#include "BrnVehicleSoaData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::VehicleSoaData::Construct
//
// Zeroes every traffic-vehicle bit set. The compiler unrolled the quadword
// clears across the whole SoA block; the loop is re-rolled here per bit array.

namespace BrnTraffic
{
namespace
{
    void ClearBitArray(u64* lpaBits)
    {
        for (int i = 0; i < 10; ++i) // 0x50-byte FastBitArray<601> block
        {
            lpaBits[i] = 0;
        }
    }
}

VehicleSoaData* VehicleSoaData::Construct()
{
    ClearBitArray(mAliveVehicles);
    ClearBitArray(mVehiclesWithEntities);
    ClearBitArray(mCollidableVehicles);
    ClearBitArray(mPhysicalVehicles);
    ClearBitArray(mArticulatedVehicles);
    ClearBitArray(mVehiclesRenderedLastFrame);
    ClearBitArray(mPhysicalVehiclesFarFromPlayer);
    ClearBitArray(mPhysicalVehiclesTryingToRecover);

    return this;
}
}
