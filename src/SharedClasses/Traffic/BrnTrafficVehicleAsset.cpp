// =============================================================================
// BrnTrafficVehicleAsset.cpp -- owning .cpp for BrnTraffic::VehicleAsset. The two
// accessors (:49 / :50) are inline in the header, as the console inlines them too.
// PARK: EndianSwap (:55) stays declared-only; see the header banner.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // Never called. Pins the serialised asset record at 8 bytes: the shipped
    // B5TRAFFIC.BNDL asset block is 27 * 8 and tiles exactly between mpaVehicleAssets
    // and mpaKillZoneIds. Pointer-free, so console and host footprints are identical.
    void VehicleAsset::_AssertLayout()
    {
        static_assert(sizeof(CgsID) == 8, "CgsID is a 64-bit compressed name id");
        static_assert(offsetof(VehicleAsset, mVehicleId) == 0, "VehicleAsset::mVehicleId @+0");
        static_assert(sizeof(VehicleAsset) == 8, "VehicleAsset stride");
    }
}
