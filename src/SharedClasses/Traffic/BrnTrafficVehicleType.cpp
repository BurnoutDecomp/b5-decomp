// =============================================================================
// BrnTrafficVehicleType.cpp  (owning .cpp for the BrnTraffic vehicle-type records)
//
// Both EndianSwap declarations (:120 / :151) are parked -- no standalone ARTIST
// symbol, no rw::EndianSwap reconstruction in this tree, and the shipped PC payload
// is already little-endian (evidence in BrnTrafficStaticTraffic.cpp). What lands here
// is the never-called layout pin for each record.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // 20 bytes, five contiguous f32. The block is 16-byte aligned on disc, which is
    // what TrafficData::FixUp's Is16Aligned(mpaVehicleTypesUpdate) assert guards --
    // that is a BLOCK alignment, not an element alignment, so the record itself stays
    // 4-aligned and 20 bytes long.
    void VehicleTypeUpdateData::_AssertLayout()
    {
        static_assert(offsetof(VehicleTypeUpdateData, mfWheelRadius)      == 0x00, "mfWheelRadius");
        static_assert(offsetof(VehicleTypeUpdateData, mfSuspensionRoll)   == 0x04, "mfSuspensionRoll");
        static_assert(offsetof(VehicleTypeUpdateData, mfSuspensionPitch)  == 0x08, "mfSuspensionPitch");
        static_assert(offsetof(VehicleTypeUpdateData, mfSuspensionTravel) == 0x0C, "mfSuspensionTravel");
        static_assert(offsetof(VehicleTypeUpdateData, mfMass)             == 0x10, "mfMass");
        static_assert(sizeof(VehicleTypeUpdateData) == 20, "VehicleTypeUpdateData stride");
    }

    // 8 bytes. TrafficData::GetVehicleTraitsForVehicleType @0x82705DF0 pins both the
    // stride (`slwi r10,r29,3`) and muTraitsId's place inside the element
    // (`lbz r24, 6(r11)`); UpdateVehicles_CreateNewVehicles @0x8273A308 pins
    // muTrailerFlowTypeId at +0 (u16 compared against 0xFFFF) and mxVehicleFlags at +2.
    void VehicleTypeData::_AssertLayout()
    {
        static_assert(offsetof(VehicleTypeData, muTrailerFlowTypeId) == 0x00, "muTrailerFlowTypeId @+0");
        static_assert(offsetof(VehicleTypeData, mxVehicleFlags)      == 0x02, "mxVehicleFlags @+2");
        static_assert(offsetof(VehicleTypeData, muVehicleClass)      == 0x03, "muVehicleClass @+3");
        static_assert(offsetof(VehicleTypeData, muInitialDirt)       == 0x04, "muInitialDirt @+4");
        static_assert(offsetof(VehicleTypeData, muAssetId)           == 0x05, "muAssetId @+5");
        static_assert(offsetof(VehicleTypeData, muTraitsId)          == 0x06, "muTraitsId @+6 (lbz 6)");
        static_assert(sizeof(VehicleTypeData) == 8, "VehicleTypeData stride (slwi r10,r29,3)");
    }
}
