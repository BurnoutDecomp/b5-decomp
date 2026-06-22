#pragma once

// =============================================================================
// BrnTrafficSection.h  (NEW OWNING HEADER -- partial)
//
// DWARF home (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficSection.h)
// of the BrnTraffic traffic-graph value types. This slice owns only:
//
//   BrnTraffic::LaneRung            (struct @ BrnTrafficSection.h:50)
//   BrnTraffic::LaneRung::GetRightVector  @ 0x821F4A88   (this TU)
//
// The X360 bakes this file's path into GetRightVector's assert
// ("...\\SharedClasses\\traffic\\BrnTrafficSection.h", line 328 of the original),
// confirming the home. The sibling value types in this DWARF file
// (Section / Neighbour / Side / the Section accessor family) are NOT reconstructed
// here -- they belong to other slices; only LaneRung is defined so this slice stays
// minimal and additive. When the full BrnTrafficSection.h lands it should GROW this
// header with those siblings, never redefine LaneRung.
//
// LAYOUT (DWARF-authoritative): LaneRung is a single member -- Vector3 maPoints[2]
// (BrnTrafficSection.h:52). Two 16-byte Vector3 lanes => 32 bytes. The X360 asm reads
// maPoints[0] at +0 (`lvx128 v13,r0,r4`) and maPoints[1] at +0x10 (`lvx128 v12,r4,r10`
// with r10=0x10), exactly the &maPoints[0]/&maPoints[1] stride this layout produces.
// =============================================================================

#include "BrnCommonTypes.h"   // Vector3 (= rw::math::vpu::Vector3)

namespace BrnTraffic
{
    // BrnTrafficSection.h:50 -- a lane "rung": the two cross-lane endpoints that span
    // one lane at one point along a section. maPoints[0]/[1] are the left/right (or
    // start/end) endpoints; their difference is the lateral "right" direction.
    struct LaneRung
    {
        // BrnTrafficSection.h:52 -- the two rung endpoints (16-byte SIMD lanes).
        Vector3 maPoints[2];

        // BrnTrafficSection.h:57 / :61 / :65 -- attested members.
        // GetCentrePos and EndianSwap live in their own (not-yet-reconstructed) TUs;
        // declared here so the home is faithful, bodied elsewhere. NOT owned by this
        // slice. GetRightVector @ 0x821F4A88 is owned and bodied in
        // BrnTrafficSection.cpp.
        Vector3 GetCentrePos() const;    // :57
        Vector3 GetRightVector() const;  // :61  (this TU, @ 0x821F4A88)
        void    EndianSwap();            // :65
    };
}
