#pragma once

// =============================================================================
// BrnTrafficFlowType.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::FlowType -- one traffic "flow": the weighted mix of vehicle
// types a section spawns. TrafficData::mpapFlowTypes is an array of FlowType*.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficFlowType.h @ :45):
//   +0x00 mpauVehicleTypeIds  P  u16*  :47
//   +0x04 mpauCumulativeProb  P  u8*   :48
//   +0x08 muNumVehicleTypes   u8       :49       (X360 sizeof == 0x0C)
//
// RELOCATION: the console has no standalone FlowType::FixUp symbol -- TrafficData::FixUp
// @0x827637D8 INLINES it (`mpapFlowTypes[i] += base; p[0] += base; p[4] += base`, with no
// null guard). The DWARF still declares FixUp/FixDown (:56 / :61), so they are de-inlined
// here and called from TrafficData's bodies.
// =============================================================================

#include "types.hpp"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // BrnTrafficFlowType.h:45
    struct FlowType
    {
        u16* mpauVehicleTypeIds;   // +0 (:47)  muNumVehicleTypes entries
        u8*  mpauCumulativeProb;   // +8 (:48)  cumulative spawn probability, same count
        u8   muNumVehicleTypes;    //    (:49)

        void FixUp(const void* lpBaseData);     // (:56)
        void FixDown(const void* lpBaseData);   // (:61)
    };

    static_assert(offsetof(FlowType, mpauCumulativeProb) == 8, "FlowType::mpauCumulativeProb");
    static_assert(offsetof(FlowType, muNumVehicleTypes) == 0x10, "FlowType::muNumVehicleTypes");
    static_assert(sizeof(FlowType) == 0x18, "FlowType host sizeof");
}
