#include "BrnVehicleSoaData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::VehicleSoaData::Construct
//
// Zeroes every traffic-vehicle bit set. The compiler unrolled the quadword
// clears across the whole SoA block; the loop is re-rolled here per bit array.

namespace BrnTraffic
{
void VehicleSoaData::Construct()
{
    mAliveVehicles.Construct();
    mVehiclesWithEntities.Construct();
    mCollidableVehicles.Construct();
    mPhysicalVehicles.Construct();
    mArticulatedVehicles.Construct();
    mVehiclesRenderedLastFrame.Construct();
    mPhysicalVehiclesFarFromPlayer.Construct();
    mPhysicalVehiclesTryingToRecover.Construct();
}
}
