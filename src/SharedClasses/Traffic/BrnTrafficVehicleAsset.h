#pragma once

// =============================================================================
// BrnTrafficVehicleAsset.h  (OWNING HEADER)
//
// Home for BrnTraffic::VehicleAsset -- the CgsID of one traffic vehicle asset.
// TrafficData::mpaVehicleAssets is an array of these (muNumVehicleAssets entries);
// Hull::mauVehicleAssets indexes it per hull, and TrafficCarStreamer::SetAssetList
// @0x82753A38 uncompresses each mVehicleId into the `VEH_T*` bundle name it streams.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficVehicleAsset.h @ :45):
// a single PRIVATE CgsID (:66) behind Get/SetVehicleId (:49 / :50), so sizeof == 8 on
// both target and host. Attested by the shipped B5TRAFFIC.BNDL block extent (27 assets
// * 8 == 0xD8, tiling exactly between mpaVehicleAssets and mpaKillZoneIds).
//
// The Feb-2007 leak carries the identical class (same member, same two inline
// accessors, same EndianSwap), so rungs 2 and 3 agree.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID

namespace BrnTraffic
{
    // BrnTrafficVehicleAsset.h:45
    struct VehicleAsset
    {
        CgsID GetVehicleId() const { return mVehicleId; }   // (:49) console-inlined
        void  SetVehicleId(CgsID lId) { mVehicleId = lId; } // (:50) console-inlined

        // (:55) DECLARED-ONLY. No standalone ARTIST symbol (it folds to nothing on a
        // big-endian target); the Feb-2007 body is one rw::EndianSwap call and
        // rw::EndianSwap has no reconstruction in b5-decomp; and the shipped PC
        // payload is already little-endian, so the correct host body is a no-op.
        // (Full endian evidence: BrnTrafficStaticTraffic.cpp.)
        void  EndianSwap();

        // Never called. A MEMBER function so the body is a complete-class context and
        // offsetof reaches the PRIVATE member. Body in BrnTrafficVehicleAsset.cpp.
        static void _AssertLayout();

    private:
        CgsID mVehicleId;   // (:66)
    };

    static_assert(sizeof(VehicleAsset) == 8, "VehicleAsset stride");
}
