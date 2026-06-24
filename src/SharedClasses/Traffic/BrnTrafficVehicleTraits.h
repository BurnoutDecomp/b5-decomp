#pragma once

// Traffic vehicle-traits record. Recovered from the DecFIGS DWARF
// (SharedClasses/Traffic/BrnTrafficVehicleTraits.h). This slice owns only the small
// serialised value type referenced by TrafficData's traits table; the accessor bodies
// (GetSwervingModifier / GetAccelerationModifier / EndianSwap) belong to other slices
// and should GROW this header additively rather than redefine the struct.
#include "types.hpp"

namespace BrnTraffic
{
    // BrnTrafficVehicleTraits.h:45 (DWARF) -- one vehicle-traits record. sizeof == 16
    // (X360-authoritative: TrafficData::GetVehicleTraitsForVehicleType @ 0x82705DF0 indexes the
    // mpaVehicleTraits table at a 16-byte stride (traitsIndex*16 == `slwi r11,r24,4`)). Field
    // order/offsets per the DWARF:
    //   mfSwervingAmountModifier f32 @+0, mfAcceleration f32 @+4, then five u8 cumulative-
    //   probability/patience bytes @+8..+12 (-> 16 with f32 4-byte alignment).
    struct VehicleTraits
    {
        f32 mfSwervingAmountModifier; // :74  +0x00
        f32 mfAcceleration;           // :75  +0x04
        u8  muCuttingUpChance;        // :77  +0x08
        u8  muTailgatingChance;       // :78  +0x09
        u8  muPatience;               // :80  +0x0A
        u8  muTantrumAttackCumProb;   // :81  +0x0B
        u8  muTantrumStopCumProb;     // :82  +0x0C  (+3 trailing pad -> 16)
    };
}
