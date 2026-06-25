#pragma once

// Traffic vehicle silhouette class. Recovered from the DecFIGS DWARF.
#include "types.hpp"

namespace BrnTraffic
{
    enum VehicleClass : s32
    {
        E_VEHICLECLASS_CAR    = 0,
        E_VEHICLECLASS_VAN    = 1,
        E_VEHICLECLASS_BUS    = 2,
        E_VEHICLECLASS_BIGRIG = 3,
        E_VEHICLECLASS_COUNT  = 4
    };

    // BrnTrafficVehicleType.h:107 (DecFIGS) -- the per-frame values derived from
    // a vehicle type's physics and graphics attributes.
    struct VehicleTypeUpdateData
    {
        f32 mfWheelRadius;
        f32 mfSuspensionRoll;
        f32 mfSuspensionPitch;
        f32 mfSuspensionTravel;
        f32 mfMass;

        void EndianSwap();
    };

    // BrnTrafficVehicleType.h:135 (DWARF) -- one serialised vehicle-type record. sizeof == 8
    // (X360-authoritative: TrafficData::GetVehicleTraitsForVehicleType @ 0x82705DF0 indexes the
    // mpaVehicleTypes table at an 8-byte stride (`slwi r10,r29,3`) and reads the traits id at
    // byte +6 within the element (`lbz r24, 6(r11)`)). Field order/offsets per the DWARF:
    //   muTrailerFlowTypeId u16 @+0, mxVehicleFlags u8 @+2, muVehicleClass u8 @+3,
    //   muInitialDirt u8 @+4, muAssetId u8 @+5, muTraitsId u8 @+6 (-> 8 with u16-alignment pad).
    struct VehicleTypeData
    {
        u16 muTrailerFlowTypeId; // :137  +0x00
        u8  mxVehicleFlags;      // :138  +0x02
        u8  muVehicleClass;      // :139  +0x03
        u8  muInitialDirt;       // :141  +0x04
        u8  muAssetId;           // :143  +0x05
        u8  muTraitsId;          // :145  +0x06  (+1 trailing pad -> 8)
    };
}
