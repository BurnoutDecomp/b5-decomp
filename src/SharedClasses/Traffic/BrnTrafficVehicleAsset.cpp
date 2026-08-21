// =============================================================================
// BrnTrafficVehicleAsset.cpp  (owning .cpp for BrnTraffic::VehicleAsset)
//
// The two accessors (:49 / :50) are inline in the header -- the console inlines them
// too, which is why neither has a standalone ARTIST symbol. EndianSwap (:55) is
// declared-only; see the header banner.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // Never called. Pins the serialised asset record.
    //   8 -- the shipped B5TRAFFIC.BNDL asset block is muNumVehicleAssets (27) * 8
    //        bytes and tiles exactly between mpaVehicleAssets and mpaKillZoneIds.
    // Pointer-free record, so the console and host footprints are identical.
    void VehicleAsset::_AssertLayout()
    {
        static_assert(sizeof(CgsID) == 8, "CgsID is a 64-bit compressed name id");
        static_assert(offsetof(VehicleAsset, mVehicleId) == 0, "VehicleAsset::mVehicleId @+0");
        static_assert(sizeof(VehicleAsset) == 8, "VehicleAsset stride");
    }
}
