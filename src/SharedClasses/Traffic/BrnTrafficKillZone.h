#pragma once

// Traffic kill-zone record types. Recovered from the DecFIGS DWARF
// (SharedClasses/Traffic/BrnTrafficKillZone.h). This slice owns only the small
// serialised value types referenced by TrafficData's kill-zone tables; the heavier
// kill-zone machinery (FindKillZone search, EndianSwap bodies) belongs to other slices
// and should GROW this header additively rather than redefine these.
#include "types.hpp"

namespace BrnTraffic
{
    // BrnTrafficKillZone.h:50 (DWARF) -- one kill-zone region: a hull plus a section/rung span.
    // sizeof == 6 (X360-authoritative: TrafficData::GetKillZoneRegions @ 0x82705D08 indexes the
    // mpaKillZoneRegions table at a 6-byte stride (index*6 == `slwi r10,r28,1; add; slwi r10,1`)).
    // Field order/offsets per the DWARF:
    //   muHull u16 @+0, muSection u8 @+2, muStartRung u8 @+3, muEndRung u8 @+4 (-> 6, u16-aligned).
    struct KillZoneRegion
    {
        u16 muHull;      // :84  +0x00
        u8  muSection;   // :85  +0x02
        u8  muStartRung; // :86  +0x03
        u8  muEndRung;   // :87  +0x04  (+1 trailing pad -> 6)
    };

    // BrnTrafficKillZone.h:102 (DWARF) -- a kill-zone: an offset into the region table plus a
    // count. Kept here as the DWARF-attested companion of KillZoneRegion (not directly indexed
    // by this slice's getters, but homed with its sibling). muOffset u16 @+0, muCount u8 @+2.
    struct KillZone
    {
        u16 muOffset; // :129  +0x00
        u8  muCount;  // :130  +0x02  (-> 4, u16-aligned)
    };
}
