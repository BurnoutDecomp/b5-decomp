#pragma once

// Traffic -> race-car shared-IO element payloads. Reconstructed from the DecFIGS DWARF
// (GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h).
//
// This is the element-type home for the traffic-to-race-car interface records. The X360
// build packs NearMissData into an Array<NearMissData,16> (the near-miss traffic collection)
// inside TrafficToRaceCarInterface_PreScene; the Array<NearMissData,16>::Append instantiation
// (X360 0x82709C00) is the per-instantiation .cpp alongside this header.
//
// Element sizes are X360-authoritative (the Array<NearMissData,16>::Append @ 0x82709C00 reads
// the live count at byte offset 0x80 == 16 * sizeof(NearMissData) and writes 2 dwords per
// element with a 3-bit << stride, i.e. sizeof(NearMissData) == 8).
#include "types.hpp"                          // u32/f32/s8/s32
#include "BrnCommonTypes.h"                   // Vector3, EntityId

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // DWARF :48 -- a traffic vehicle the player could "stomp" (drive over). sizeof == 32
    // (Vector3 SIMD(16) + EntityId(4) + f32(4) -> 24, padded to 32 by the Vector3 16-align).
    struct alignas(16) VehicleStompingData
    {
        Vector3  mStompeePosition;   // :50
        EntityId mStompeeEntityId;   // :51
        f32      mfDistanceSquared;  // :52
    };

    // DWARF :57 -- one nearby traffic/race-car proximity record. sizeof == 8 (X360-authoritative:
    // Array<NearMissData,16>::Append @ 0x82709C00 writes 2 dwords per element with an 8-byte stride).
    struct NearMissData
    {
        u32 muCarId;     // :59
        f32 mfDistance;  // :60
    };
}
}
