#ifndef BRN_VEHICLE_SOA_DATA_H
#define BRN_VEHICLE_SOA_DATA_H

#include "types.hpp"

namespace BrnTraffic
{
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x827xxxxx.
// Structure-of-arrays traffic vehicle data; Construct zeroes all 80 quadwords.
class VehicleSoaData
{
public:
    VehicleSoaData* Construct();

private:
    u64 mData[80]; // guest [0..79], 8 bytes each
};
}

#endif
