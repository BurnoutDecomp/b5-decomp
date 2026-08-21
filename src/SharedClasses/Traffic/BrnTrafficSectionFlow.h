#pragma once

// =============================================================================
// BrnTrafficSectionFlow.h  (NEW OWNING HEADER -- wave T1, cluster C2)
//
// Home for BrnTraffic::SectionFlow -- the per-SECTION spawn rate: which flow type a
// lane section feeds from and how many vehicles per minute it wants. Hull owns one
// array of these (Hull::mpaSectionFlows, one entry per section) and the driving-half
// generator rebuild reads it; the parked-car half does not, so this header lands now
// only to give Hull::mpaSectionFlows a real pointee instead of an opaque forward
// declaration.
//
// LAYOUT is DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficSectionFlow.h @ :45):
//   +0x00 muFlowTypeId        u16  :47
//   +0x02 muVehiclesPerMinute u16  :48
// The Feb-2007 leak agrees member-for-member and pins the record at 4 bytes
// (CheckClassSize<SectionFlow,4,...>). Two u16s and no pointers, so the footprint is
// identical on the 4-byte-pointer console and the 8-byte-pointer host.
//
// SHIPPED-DATA CORROBORATION (B5TRAFFIC.BNDL, X360 original, 2026-08-21): hull 4 has
// muNumSections == 4 and its mpaSectionFlows block runs 0x3D40..0x3D50, exactly 16
// bytes == 4 * 4 -- one 4-byte record per section, no padding. (Hulls whose section
// count leaves the block short of the next block's 16-byte alignment simply pad, e.g.
// hulls 2/3/5 with 2 sections occupy 8 live bytes inside a 16-byte slot.)
//
// EndianSwap (DWARF :53) is DECLARED-ONLY, the same call this directory already makes
// for LaneRung::EndianSwap (BrnTrafficSection.h) and VehicleAsset::EndianSwap: there
// is no standalone ARTIST symbol (it folds to nothing on a big-endian target),
// rw::EndianSwap has no reconstruction in b5-decomp, and the shipped PC payload is
// already little-endian so the correct host body is a no-op.
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

        // Never called; body + evidence in BrnTrafficSectionFlow.cpp.
        static void _AssertLayout();
    };

    static_assert(sizeof(SectionFlow) == 4, "SectionFlow stride (one record per section)");
}
