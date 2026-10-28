// =============================================================================
// BrnTrafficVehicleType.cpp -- owning .cpp for the BrnTraffic vehicle-type records.
// What lands here is the never-called layout pin for each record.
// PARK: both EndianSwap declarations (:120 / :151) stay declared-only -- no ARTIST
// symbol, no rw::EndianSwap in this tree, and the shipped PC payload is already
// little-endian (BrnTrafficStaticTraffic.cpp).
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // 20 bytes, five contiguous f32. TrafficData::FixUp's
    // Is16Aligned(mpaVehicleTypesUpdate) guards the BLOCK, not the element, so the
    // record itself stays 4-aligned and 20 bytes long.
    void VehicleTypeUpdateData::_AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(VehicleTypeUpdateData, mfWheelRadius)      == 0x00), "mfWheelRadius");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeUpdateData, mfSuspensionRoll)   == 0x04), "mfSuspensionRoll");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeUpdateData, mfSuspensionPitch)  == 0x08), "mfSuspensionPitch");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeUpdateData, mfSuspensionTravel) == 0x0C), "mfSuspensionTravel");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeUpdateData, mfMass)             == 0x10), "mfMass");
        CGS_STATIC_ASSERT((sizeof(VehicleTypeUpdateData) == 20), "VehicleTypeUpdateData stride");
    }

    // 8 bytes. TrafficData::GetVehicleTraitsForVehicleType @0x82705DF0 pins both the
    // stride (`slwi r10,r29,3`) and muTraitsId's place inside the element
    // (`lbz r24, 6(r11)`); UpdateVehicles_CreateNewVehicles @0x8273A308 pins
    // muTrailerFlowTypeId at +0 (u16 compared against 0xFFFF) and mxVehicleFlags at +2.
    void VehicleTypeData::_AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, muTrailerFlowTypeId) == 0x00), "muTrailerFlowTypeId @+0");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, mxVehicleFlags)      == 0x02), "mxVehicleFlags @+2");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, muVehicleClass)      == 0x03), "muVehicleClass @+3");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, muInitialDirt)       == 0x04), "muInitialDirt @+4");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, muAssetId)           == 0x05), "muAssetId @+5");
        CGS_STATIC_ASSERT((offsetof(VehicleTypeData, muTraitsId)          == 0x06), "muTraitsId @+6 (lbz 6)");
        CGS_STATIC_ASSERT((sizeof(VehicleTypeData) == 8), "VehicleTypeData stride (slwi r10,r29,3)");
    }
}
