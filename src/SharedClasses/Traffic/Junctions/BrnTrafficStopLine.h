#pragma once

// =============================================================================
// BrnTrafficStopLine.h  (NEW OWNING HEADER)
//
// Home for BrnTraffic::StopLine -- the per-junction stop-line value type of the
// TrafficData traffic-graph (forward-declared as `struct StopLine;` in
// BrnTrafficHull.h, where the hull holds `StopLine* mpaStopLines`).
//
// The X360 retail XEX bakes this exact header path into ConvertToFixed's assert
// string ("...sharedclasses\\traffic\\Junctions/BrnTrafficStopLine.h", line 117),
// which fixes the home directory + file name.
//
// This slice owns ONLY the single attested standalone helper:
//   BrnTraffic::StopLine::ConvertToFixed  @ 0x8274F3C8
//
// ConvertToFixed is a static utility (no `this` use in the asm -- the float comes
// in f1 and the only memory touched is the spill slot for the fctidz result). It
// converts a lane-parameter float in [0, 256) into an 8.8 unsigned fixed-point
// value, the on-disk packing the stop-line records use for their lane parameter.
//
// The StopLine struct's own data members are NOT pinned by this TU (the only
// attested function here is the static converter; the members are written/read in
// other TUs -- e.g. BrnTraffic::Section::FindNextStopLineIndex). They are left to
// the StopLine-layout TU to define; this header declares the type and the one
// attested static method only, additively, so the layout TU can GROW it later.
// =============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnTraffic
{
    // BrnTrafficStopLine.h:117 (X360 assert site) -- per-junction stop-line record.
    // Members are defined by the StopLine-layout TU; this slice contributes only the
    // attested static converter so the forward-declared `StopLine` in BrnTrafficHull.h
    // gains its declared home.
    struct StopLine
    {
        // ConvertToFixed @ 0x8274F3C8
        // Pack a lane parameter (a float, asserted < 256.0f) into 8.8 unsigned fixed
        // point: multiply by 256.0f, truncate toward zero, keep the low 16 bits.
        //   asm: assert(lfFloat < 256.0f);
        //        return (u16)(s64)(lfFloat * 256.0f);   // fmuls + fctidz + low halfword
        static u16 ConvertToFixed(f32 lfFloat);
    };
}
