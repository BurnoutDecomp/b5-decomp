#include "BrnVehicleSoaData.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::VehicleSoaData::Construct
//
// Zeroes the entire structure-of-arrays block. The compiler unrolled the
// quadword clears; the loop is re-rolled here.

namespace BrnTraffic
{
VehicleSoaData* VehicleSoaData::Construct()
{
    for (int i = 0; i < 80; ++i)
    {
        mData[i] = 0;
    }

    return this;
}
}
