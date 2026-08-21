#pragma once

// =============================================================================
// BrnTraffic::SectionFlow -- the per-SECTION spawn rate: which flow type a lane
// section feeds from and how many vehicles per minute it wants. Hull::mpaSectionFlows
// is one array of these, one entry per section.
//
// Layout is DWARF-authoritative (dwarfdump/.../BrnTrafficSectionFlow.h @ :45): two
// u16s, no pointers, so the 4-byte footprint is identical on console and host. The
// shipped B5TRAFFIC.BNDL corroborates: hull 4 has 4 sections and its mpaSectionFlows
// block runs 0x3D40..0x3D50, exactly 4 * 4 with no padding.
//
// PARK: EndianSwap (DWARF :53) stays declared-only, as it does for LaneRung and
// VehicleAsset in this directory. No ARTIST symbol, no rw::EndianSwap in this tree,
// and the shipped PC payload is already little-endian.
// =============================================================================

#include "types.hpp"

namespace BrnTraffic
{
    // BrnTrafficSectionFlow.h:45
    struct SectionFlow
    {
        u16 muFlowTypeId;         // :47  +0x00 -- index into TrafficData::mpapFlowTypes,
                                  //              or KU_INVALID_FLOWTYPE (0xFFFF)
        u16 muVehiclesPerMinute;  // :48  +0x02 -- generator rate for this section

        // DWARF :53. DECLARED-ONLY -- see the header banner.
        void EndianSwap();

        // Never called; body in BrnTrafficSectionFlow.cpp.
        static void _AssertLayout();
    };

    static_assert(sizeof(SectionFlow) == 4, "SectionFlow stride (one record per section)");
}
