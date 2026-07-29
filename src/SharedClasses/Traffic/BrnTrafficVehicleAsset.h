#pragma once

// =============================================================================
// BrnTrafficVehicleAsset.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::VehicleAsset -- the CgsID of one traffic vehicle asset.
// TrafficData::mpaVehicleAssets is an array of these (muNumVehicleAssets entries);
// Hull::mauVehicleAssets indexes it per hull.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficVehicleAsset.h @ :45):
// a single private CgsID (:66), so sizeof == 8 on both target and host. Attested by the
// shipped B5TRAFFIC.BNDL block extent (27 assets * 8 == 0xD8, tiling exactly between
// mpaVehicleAssets and mpaKillZoneIds).
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

namespace BrnTraffic
{
    // BrnTrafficVehicleAsset.h:45
    struct VehicleAsset
    {
        CgsID GetVehicleId() const { return mVehicleId; }   // (:49)
        void  SetVehicleId(CgsID lId) { mVehicleId = lId; } // (:50)
        void  EndianSwap();                                 // (:55) own TU

    private:
        CgsID mVehicleId;   // (:66)
    };

    static_assert(sizeof(VehicleAsset) == 8, "VehicleAsset stride");
}
