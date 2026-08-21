#pragma once

// =============================================================================
// BrnTraffic::VehicleAsset -- the CgsID of one traffic vehicle asset.
// TrafficData::mpaVehicleAssets is an array of these; Hull::mauVehicleAssets indexes
// it per hull, and TrafficCarStreamer::SetAssetList @0x82753A38 uncompresses each
// mVehicleId into the VEH_T* bundle name it streams.
//
// Layout is DWARF-authoritative (dwarfdump/.../BrnTrafficVehicleAsset.h @ :45): one
// private CgsID (:66) behind Get/SetVehicleId (:49 / :50), so sizeof is 8 on target
// and host alike. The shipped B5TRAFFIC.BNDL asset block attests it: 27 assets * 8 ==
// 0xD8, tiling exactly between mpaVehicleAssets and mpaKillZoneIds.
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

        // (:55) DECLARED-ONLY. No ARTIST symbol, no rw::EndianSwap in this tree, and the
        // shipped PC payload is already little-endian, so the correct host body is a
        // no-op. Endian evidence: BrnTrafficStaticTraffic.cpp.
        void  EndianSwap();

        // Never called. A member function so offsetof reaches the private member.
        static void _AssertLayout();

    private:
        CgsID mVehicleId;   // (:66)
    };

    static_assert(sizeof(VehicleAsset) == 8, "VehicleAsset stride");
}
